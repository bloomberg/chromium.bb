// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ABOUT_FLAGS_H_
#pragma once

#include <string>

class CommandLine;
class ListValue;
class PrefService;

namespace about_flags {

// Returns if Flags is enabled (it isn't for ChromeOS at the moment).
bool IsEnabled();

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

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
