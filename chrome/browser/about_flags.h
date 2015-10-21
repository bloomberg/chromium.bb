// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ABOUT_FLAGS_H_
#define CHROME_BROWSER_ABOUT_FLAGS_H_

#include <stdint.h>

#include <map>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/strings/string16.h"

class PrefService;

namespace base {
class ListValue;
}

namespace flags_ui {
class FlagsStorage;
}

namespace about_flags {

// FeatureEntry is used by about_flags to describe an experimental feature.
//
// Note that features should eventually be either turned on by default with no
// about_flags entries or deleted. Most feature entries should only be around
// for a few milestones, until their full launch.
//
// This is exposed only for testing.
struct FeatureEntry {
  enum Type {
    // A feature with a single flag value. This is typically what you want.
    SINGLE_VALUE,

    // A default enabled feature with a single flag value to disable it. Please
    // consider whether you really need a flag to disable the feature, and even
    // if so remove the disable flag as soon as it is no longer needed.
    SINGLE_DISABLE_VALUE,

    // The feature has multiple values only one of which is ever enabled.
    // The first of the values should correspond to a deactivated state for this
    // feature (i.e. no command line option). For MULTI_VALUE entries, the
    // command_line of the FeatureEntry is not used. If the experiment is
    // enabled the command line of the selected Choice is enabled.
    MULTI_VALUE,

    // The feature has three possible values: Default, Enabled and Disabled.
    // This should be used for features that may have their own logic to decide
    // if the feature should be on when not explicitly specified via about
    // flags - for example via FieldTrials.
    ENABLE_DISABLE_VALUE,

    // Corresponds to a base::Feature, per base/feature_list.h. The entry will
    // have three states: Default, Enabled, Disabled. When not specified or set
    // to Default, the normal default value of the feature is used.
    FEATURE_VALUE,
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

  // The internal name of the feature entry. This is never shown to the user.
  // It _is_ however stored in the prefs file, so you shouldn't change the
  // name of existing flags.
  const char* internal_name;

  // String id of the message containing the feature's name.
  int visible_name_id;

  // String id of the message containing the feature's description.
  int visible_description_id;

  // The platforms the feature is available on.
  // Needs to be more than a compile-time #ifdef because of profile sync.
  unsigned supported_platforms;  // bitmask

  // Type of entry.
  Type type;

  // The commandline switch and value that are added when this flag is active.
  // This is different from |internal_name| so that the commandline flag can be
  // renamed without breaking the prefs file.
  // This is used if type is SINGLE_VALUE or ENABLE_DISABLE_VALUE.
  const char* command_line_switch;
  // Simple switches that have no value should use "" for command_line_value.
  const char* command_line_value;

  // For ENABLE_DISABLE_VALUE, the command line switch and value to explicitly
  // disable the feature.
  const char* disable_command_line_switch;
  const char* disable_command_line_value;

  // For FEATURE_VALUE, the name of the base::Feature this entry corresponds to.
  const char* feature_name;

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

// Reads the state from |flags_storage| and adds the command line flags
// belonging to the active feature entries to |command_line|.
void ConvertFlagsToSwitches(flags_ui::FlagsStorage* flags_storage,
                            base::CommandLine* command_line,
                            SentinelsMode sentinels);

// Compares a set of switches of the two provided command line objects and
// returns true if they are the same and false otherwise.
// If |out_difference| is not NULL, it's filled with set_symmetric_difference
// between sets.
bool AreSwitchesIdenticalToCurrentCommandLine(
    const base::CommandLine& new_cmdline,
    const base::CommandLine& active_cmdline,
    std::set<base::CommandLine::StringType>* out_difference);

// Differentiate between generic flags available on a per session base and flags
// that influence the whole machine and can be said by the admin only. This flag
// is relevant for ChromeOS for now only and dictates whether entries marked
// with the |kOsCrOSOwnerOnly| label should be enabled in the UI or not.
enum FlagAccess { kGeneralAccessFlagsOnly, kOwnerAccessToFlags };

// Gets the list of feature entries. Entries that are available for the current
// platform are appended to |supported_entries|; all other entries are appended
// to |unsupported_entries|.
void GetFlagFeatureEntries(flags_ui::FlagsStorage* flags_storage,
                           FlagAccess access,
                           base::ListValue* supported_entries,
                           base::ListValue* unsupported_entries);

// Returns true if one of the feature entry flags has been flipped since
// startup.
bool IsRestartNeededToCommitChanges();

// Enables or disables the current with id |internal_name|.
void SetFeatureEntryEnabled(flags_ui::FlagsStorage* flags_storage,
                            const std::string& internal_name,
                            bool enable);

// Removes all switches that were added to a command line by a previous call to
// |ConvertFlagsToSwitches()|.
void RemoveFlagsSwitches(
    std::map<std::string, base::CommandLine::StringType>* switch_list);

// Reset all flags to the default state by clearing all flags.
void ResetAllFlags(flags_ui::FlagsStorage* flags_storage);

// Returns the value for the current platform. This is one of the values defined
// by the OS enum above.
// This is exposed only for testing.
int GetCurrentPlatform();

// Sends UMA stats about experimental flag usage. This should be called once per
// startup.
void RecordUMAStatistics(flags_ui::FlagsStorage* flags_storage);

// Returns the UMA id for the specified switch name.
base::HistogramBase::Sample GetSwitchUMAId(const std::string& switch_name);

// Sends stats (as UMA histogram) about command_line_difference.
// This is used on ChromeOS to report flags that lead to browser restart.
// |command_line_difference| is the result of
// AreSwitchesIdenticalToCurrentCommandLine().
void ReportCustomFlags(const std::string& uma_histogram_hame,
                       const std::set<std::string>& command_line_difference);

namespace testing {

// Clears internal global state, for unit tests.
void ClearState();

// Sets the list of feature entries. Pass in null to use the default set. This
// does NOT take ownership of the supplied |entries|.
void SetFeatureEntries(const FeatureEntry* entries, size_t count);

// Returns the current set of feature entries.
const FeatureEntry* GetFeatureEntries(size_t* count);

// Separator used for multi values. Multi values are represented in prefs as
// name-of-experiment + kMultiSeparator + selected_index.
extern const char kMultiSeparator[];

// This value is reported as switch histogram ID if switch name has unknown
// format.
extern const base::HistogramBase::Sample kBadSwitchFormatHistogramId;

}  // namespace testing

}  // namespace about_flags

#endif  // CHROME_BROWSER_ABOUT_FLAGS_H_
