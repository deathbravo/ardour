/*
    Copyright (C) 2009 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef COMPILER_MSVC
#include <io.h> // Microsoft's nearest equivalent to <unistd.h>
#include <ardourext/misc.h>
#else
#include <regex.h>
#endif

#include "pbd/strsplit.h"
#include "pbd/convert.h"

#include "ardour/controllable_descriptor.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

int
ControllableDescriptor::set (const std::string& str)
{
	string::size_type first_space = str.find_first_of (" ");

	if (first_space == string::npos) {
		return -1;
	}

	string front = str.substr (0, first_space);
	string back = str.substr (first_space);

	vector<string> path;
	split (front, path, '/');

	if (path.size() < 2) {
		return -1;
	}

	vector<string> rest;
	split (back, rest, ' ');

	if (rest.size() < 1) {
		return -1;
	}

	bool stripable = false;
	regex_t compiled_pattern;
	const char * const pattern = "^[BS]?[0-9]+";

	if (path[0] == "route" || path[0] == "rid") {

		/* this is not going to fail */
		regcomp (&compiled_pattern, pattern, REG_EXTENDED|REG_NOSUB);
		bool matched = (regexec (&compiled_pattern, rest[0].c_str(), 0, 0, 0) == 0);
		regfree (&compiled_pattern);

		if (matched) {
			_top_level_type = PresentationOrderRoute;
			stripable = true;
		} else {
			_top_level_type = NamedRoute;
			_top_level_name = rest[0];
		}

	} else if (path[0] == "vca") {
		_top_level_type = PresentationOrderVCA;
		stripable = true;
	} else if (path[0] == "bus") {
		/* digits, or B<digits> or S<digits> will be used as for route;
		   anything else will be treated as a track name.
		*/

		/* this is not going to fail */

		regcomp (&compiled_pattern, pattern, REG_EXTENDED|REG_NOSUB);
		bool matched = (regexec (&compiled_pattern, rest[0].c_str(), 0, 0, 0) == 0);
		regfree (&compiled_pattern);

		if (matched) {
			_top_level_type = PresentationOrderBus;
			stripable = true;
		} else {
			_top_level_type = NamedRoute;
			_top_level_name = rest[0];
		}

	} else if (path[0] == "track") {

		/* digits, or B<digits> or S<digits> will be used as for route;
		   anything else will be treated as a track name.
		*/

		/* this is not going to fail */
		regcomp (&compiled_pattern, pattern, REG_EXTENDED|REG_NOSUB);
		bool matched = (regexec (&compiled_pattern, rest[0].c_str(), 0, 0, 0) == 0);
		regfree (&compiled_pattern);

		if (matched) {
			_top_level_type = PresentationOrderTrack;
			stripable = true;
		} else {
			_top_level_type = NamedRoute;
			_top_level_name = rest[0];
		}
	}

	if (stripable) {
		if (rest[0][0] == 'B') {
			_banked = true;
			_presentation_order = atoi (rest[0].substr (1));
		} else if (rest[0][0] == 'S') {
			_top_level_type = SelectionCount;
			_banked = true;
			_selection_id = atoi (rest[0].substr (1));
		} else if (isdigit (rest[0][0])) {
			_banked = false;
			_presentation_order = atoi (rest[0]);
		} else {
			return -1;
		}

		_presentation_order -= 1; /* order is zero-based, but maps use 1-based */
	}

	if (path[1] == "gain") {
		_subtype = GainAutomation;

	} else if (path[1] == "trim") {
		_subtype = TrimAutomation;

	} else if (path[1] == "solo") {
		_subtype = SoloAutomation;

	} else if (path[1] == "mute") {
		_subtype = MuteAutomation;

	} else if (path[1] == "recenable") {
		_subtype = RecEnableAutomation;

	} else if (path[1] == "panwidth") {
		_subtype = PanWidthAutomation;

	} else if (path[1] == "pandirection" || path[1] == "balance") {
		_subtype = PanAzimuthAutomation;

	} else if (path[1] == "plugin") {
		if (path.size() == 3 && rest.size() == 3) {
			if (path[2] == "parameter") {
				_subtype = PluginAutomation;
				_target.push_back (atoi (rest[1]));
				_target.push_back (atoi (rest[2]));
			} else {
				return -1;
			}
		} else {
			return -1;
		}
	} else if (path[1] == "send") {

		if (path.size() == 3 && rest.size() == 2) {
			if (path[2] == "gain") {
				_subtype = SendLevelAutomation;
				_target.push_back (atoi (rest[1]));

			} else if (path[2] == "direction") {
				_subtype = SendAzimuthAutomation;
				_target.push_back (atoi (rest[1]));
			} else if (path[2] == "enable") {
				_subtype = SendEnableAutomation;
				_target.push_back (atoi (rest[1]));
			} else {
				return -1;

			}
		} else {
			return -1;
		}
	} else if (path[1] == "eq") {

		/* /route/eq/gain/<band> */

		if (path.size() != 3) {
			return -1;
		}

		_target.push_back (atoi (path[3])); /* band number */

		if (path[2] == "enable") {
			_subtype = EQEnableAutomation;
		} else if (path[2] == "gain") {
			_subtype = EQGainAutomation;
		} else if (path[2] == "freq") {
			_subtype = EQFreqAutomation;
		} else if (path[2] == "q") {
			_subtype = EQQAutomation;
		} else if (path[2] == "shape") {
			_subtype = EQShapeAutomation;
		} else {
			return -1;
		}

		/* get desired band number */
		_target.push_back (atoi (rest[1]));

	} else if (path[1] == "filter") {

		/* /route/filter/hi/freq */

		if (path.size() != 4) {
			return -1;
		}

		if (path[2] == "hi") {
			_target.push_back (1); /* high pass filter */
		} else {
			_target.push_back (0); /* low pass filter */
		}

		if (path[3] == "enable") {
			_subtype = FilterFreqAutomation;
		} else if (path[3] == "freq") {
			_subtype = FilterFreqAutomation;
		} else if (path[3] == "slope") {
			_subtype = FilterSlopeAutomation;
		} else {
			return -1;
		}

		_target.push_back (atoi (rest[1]));

	} else if (path[1] == "compressor") {

		if (path.size() != 3) {
			return -1;
		}

		if (path[2] == "enable") {
			_subtype = CompressorEnableAutomation;
		} else if (path[2] == "threshold") {
			_subtype = CompressorThresholdAutomation;
		} else if (path[2] == "mode") {
			_subtype = CompressorModeAutomation;
		} else if (path[2] == "speed") {
			_subtype = CompressorSpeedAutomation;
		} else if (path[2] == "makeup") {
			_subtype = CompressorMakeupAutomation;
		} else {
			return -1;
		}
	}

	return 0;
}

uint32_t
ControllableDescriptor::presentation_order () const
{
	if (banked()) {
		return _presentation_order + _bank_offset;
	}

	return _presentation_order;
}

uint32_t
ControllableDescriptor::selection_id () const
{
	if (banked()) {
		return _selection_id + _bank_offset;
	}

	return _selection_id;
}

uint32_t
ControllableDescriptor::target (uint32_t n) const
{
	if (n < _target.size()) {
		return _target[n];
	}

	return 0;
}
