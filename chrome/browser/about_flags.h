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

// Enumeration of OSs.
// This is exposed only for testing.
enum { kOsMac = 1 << 0, kOsWin = 1 << 1, kOsLinux = 1 << 2 , kOsCrOS = 1 << 3 };

// Experiment is used internally by about_flags to describe an experiment (and
// for testing).
// This is exposed only for testing.
struct Experiment {
  // The internal name of the experiment. This is never shown to the user.
  // It _is_ however stored in the prefs file, so you shouldn't change the
  // name of existing flags.
  const char* internal_name;

  // String id of the message containing the experiment's name.
  int visible_name_id;

  // String id of the message containing the experiment's description.
  int visible_description_id;

  // The platforms the experiment is available on
  // Needs to be more than a compile-time #ifdef because of profile sync.
  unsigned supported_platforms;  // bitmask

  // The commandline parameter that's added when this lab is active. This is
  // different from |internal_name| so that the commandline flag can be
  // renamed without breaking the prefs file.
  const char* command_line;
};

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

// Returns the value for the current platform. This is one of the values defined
// by the OS enum above.
// This is exposed only for testing.
int GetCurrentPlatform();

namespace testing {
// Clears internal global state, for unit tests.
void ClearState();

// Sets the list of experiments. Pass in NULL to use the default set. This does
// NOT take ownership of the supplied Experiments.
void SetExperiments(const Experiment* e, size_t count);
}  // namespace testing

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
