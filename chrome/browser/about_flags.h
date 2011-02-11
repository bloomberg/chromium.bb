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
  enum Type {
    // An experiment with a single value. This is typically what you want.
    SINGLE_VALUE,

    // The experiment has multiple values only one of which is ever enabled.
    // The first of the values should correspond to a deactivated state for this
    // lab (i.e. no command line option). For MULTI_VALUE experiments the
    // command_line of the Experiment is not used. If the experiment is enabled
    // the command line of the selected Choice is enabled.
    MULTI_VALUE,
  };

  // Used for MULTI_VALUE types to describe one of the possible values the user
  // can select.
  struct Choice {
    // ID of the message containing the choice name.
    int description_id;

    // Command line switch and value to enabled for this choice.
    const char* command_line_switch;
    // Simple switches that have no value should use "" for command_line_value.
    const char* command_line_value;
  };

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

  // Type of experiment.
  Type type;

  // The commandline switch and value that are added when this lab is active.
  // This is different from |internal_name| so that the commandline flag can be
  // renamed without breaking the prefs file.
  // This is used if type is SINGLE_VALUE.
  const char* command_line_switch;
  // Simple switches that have no value should use "" for command_line_value.
  const char* command_line_value;

  // This is used if type is MULTI_VALUE.
  const Choice* choices;

  // Number of |choices|.
  // This is used if type is MULTI_VALUE.
  int num_choices;
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

// Sends UMA stats about experimental flag usage. This should be called once per
// startup.
void RecordUMAStatistics(const PrefService* prefs);

namespace testing {
// Clears internal global state, for unit tests.
void ClearState();

// Sets the list of experiments. Pass in NULL to use the default set. This does
// NOT take ownership of the supplied Experiments.
void SetExperiments(const Experiment* e, size_t count);

// Returns the current set of experiments.
const Experiment* GetExperiments(size_t* count);

// Separator used for multi values. Multi values are represented in prefs as
// name-of-experiment + kMultiSeparator + selected_index.
extern const char kMultiSeparator[];

}  // namespace testing

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
