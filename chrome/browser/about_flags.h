// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ABOUT_FLAGS_H_
#pragma once

#include <map>
#include <string>

#include "base/command_line.h"

class ListValue;
class PrefService;

namespace about_flags {

// Reads the Labs |prefs| (called "Labs" for historical reasons) and adds the
// commandline flags belonging to the active experiments to |command_line|.
void ConvertFlagsToSwitches(PrefService* prefs, CommandLine* command_line);

// Get a list of all available experiments. The caller owns the result.
ListValue* GetFlagsExperimentsData(PrefService* prefs);

// Returns true if one of the experiment flags has been flipped since startup.
bool IsRestartNeededToCommitChanges();

// Enables or disables the experiment with id |internal_name|.
void SetExperimentEnabled(
    PrefService* prefs, const std::string& internal_name, bool enable);

// Removes all switches that were added to a command line by a previous call to
// |ConvertFlagsToSwitches()|.
void RemoveFlagsSwitches(
    std::map<std::string, CommandLine::StringType>* switch_list);

namespace testing {
// Clears internal global state, for unit tests.
void ClearState();
}  // namespace testing

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
