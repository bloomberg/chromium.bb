// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LABS_H_
#define CHROME_BROWSER_LABS_H_
#pragma once

#include <string>

class CommandLine;
class ListValue;
class PrefService;

// Can't be called "labs", that collides with the C function |labs()|.
namespace about_labs {

// Returns if Labs is enabled (it isn't on the stable channel).
bool IsEnabled();

// Reads the Labs |prefs| and adds the commandline flags belonging
// to the active experiments to |command_line|.
void ConvertLabsToSwitches(PrefService* prefs, CommandLine* command_line);

// Get a list of all available experiments. The caller owns the result.
ListValue* GetLabsExperimentsData(PrefService* prefs);

// Returns true if one of the experiment flags has been flipped since startup.
bool IsRestartNeededToCommitChanges();

// Enables or disables the experiment with id |internal_name|.
void SetExperimentEnabled(
    PrefService* prefs, const std::string& internal_name, bool enable);

}  // namespace Labs

#endif  // CHROME_BROWSER_LABS_H_
