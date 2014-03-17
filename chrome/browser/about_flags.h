// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ABOUT_FLAGS_H_

#include <map>
#include <string>

#include "base/command_line.h"
#include "base/strings/string16.h"

class PrefService;

namespace base {
class ListValue;
}

namespace about_flags {

class FlagsStorage;

// Enumeration of OSs.
// This is exposed only for testing.
enum { kOsMac = 1 << 0, kOsWin = 1 << 1, kOsLinux = 1 << 2 , kOsCrOS = 1 << 3,
       kOsAndroid = 1 << 4, kOsCrOSOwnerOnly = 1 << 5 };

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

    // The experiment has three possible values: Default, Enabled and Disabled.
    // This should be used for experiments that may have their own logic to
    // decide if the feature should be on when not explicitly specified via
    // about flags - for example via FieldTrials.
    ENABLE_DISABLE_VALUE,
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

  // The commandline switch and value that are added when this flag is active.
  // This is different from |internal_name| so that the commandline flag can be
  // renamed without breaking the prefs file.
  // This is used if type is SINGLE_VALUE or ENABLE_DISABLE_VALUE.
  const char* command_line_switch;
  // Simple switches that have no value should use "" for command_line_value.
  const char* command_line_value;

  // For ENABLE_DISABLE_VALUE, the command line switch and value to explictly
  // disable the feature.
  const char* disable_command_line_switch;
  const char* disable_command_line_value;

  // This is used if type is MULTI_VALUE.
  const Choice* choices;

  // Number of |choices|.
  // This is used if type is MULTI_VALUE.
  int num_choices;

  // Returns the name used in prefs for the choice at the specified |index|.
  std::string NameForChoice(int index) const;

  // Returns the human readable description for the choice at |index|.
  base::string16 DescriptionForChoice(int index) const;
};

// A flag controlling the behavior of the |ConvertFlagsToSwitches| function -
// whether it should add the sentinel switches around flags.
enum SentinelsMode { kNoSentinels, kAddSentinels };

// Reads the Labs |prefs| (called "Labs" for historical reasons) and adds the
// commandline flags belonging to the active experiments to |command_line|.
void ConvertFlagsToSwitches(FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            SentinelsMode sentinels);

// Compares a set of switches of the two provided command line objects and
// returns true if they are the same and false otherwise.
bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline);

// Differentiate between generic flags available on a per session base and flags
// that influence the whole machine and can be said by the admin only. This flag
// is relevant for ChromeOS for now only and dictates whether entries marked
// with the |kOsCrOSOwnerOnly| label should be enabled in the UI or not.
enum FlagAccess { kGeneralAccessFlagsOnly, kOwnerAccessToFlags };

// Get the list of experiments. Experiments that are available on the current
// platform are appended to |supported_experiments|; all other experiments are
// appended to |unsupported_experiments|.
void GetFlagsExperimentsData(FlagsStorage* flags_storage,
                             FlagAccess access,
                             base::ListValue* supported_experiments,
                             base::ListValue* unsupported_experiments);

// Returns true if one of the experiment flags has been flipped since startup.
bool IsRestartNeededToCommitChanges();

// Enables or disables the experiment with id |internal_name|.
void SetExperimentEnabled(FlagsStorage* flags_storage,
                          const std::string& internal_name,
                          bool enable);

// Removes all switches that were added to a command line by a previous call to
// |ConvertFlagsToSwitches()|.
void RemoveFlagsSwitches(
    std::map<std::string, base::CommandLine::StringType>* switch_list);

// Reset all flags to the default state by clearing all flags.
void ResetAllFlags(FlagsStorage* flags_storage);

// Returns the value for the current platform. This is one of the values defined
// by the OS enum above.
// This is exposed only for testing.
int GetCurrentPlatform();

// Sends UMA stats about experimental flag usage. This should be called once per
// startup.
void RecordUMAStatistics(FlagsStorage* flags_storage);

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
