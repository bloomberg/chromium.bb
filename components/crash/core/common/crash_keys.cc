// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"

namespace crash_keys {

namespace {

#if defined(OS_MACOSX) || defined(OS_WIN)
// When using Crashpad, the crash reporting client ID is the responsibility of
// Crashpad. It is not set directly by Chrome. To make the metrics client ID
// available on the server, it's stored in a distinct key.
const char kMetricsClientId[] = "metrics_client_id";
#else
// When using Breakpad instead of Crashpad, the crash reporting client ID is the
// same as the metrics client ID.
const char kMetricsClientId[] = "guid";
#endif

crash_reporter::CrashKeyString<40> client_id_key(kMetricsClientId);

}  // namespace

const char kNumVariations[] = "num-experiments";
const char kVariations[] = "variations";

void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid) {
  std::string stripped_guid(metrics_client_guid);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  base::ReplaceSubstringsAfterOffset(
      &stripped_guid, 0, "-", base::StringPiece());
  if (stripped_guid.empty())
    return;

  client_id_key.Set(stripped_guid);
}

void ClearMetricsClientId() {
#if defined(OS_MACOSX) || defined(OS_WIN)
  // Crashpad always monitors for crashes, but doesn't upload them when
  // crash reporting is disabled. The preference to upload crash reports is
  // linked to the preference for metrics reporting. When metrics reporting is
  // disabled, don't put the metrics client ID into crash dumps. This way, crash
  // reports that are saved but not uploaded will not have a metrics client ID
  // from the time that metrics reporting was disabled even if they are uploaded
  // by user action at a later date.
  //
  // Breakpad cannot be enabled or disabled without an application restart, and
  // it needs to use the metrics client ID as its stable crash client ID, so
  // leave its client ID intact even when metrics reporting is disabled while
  // the application is running.
  client_id_key.Clear();
#endif
}

void SetVariationsList(const std::vector<std::string>& variations) {
  base::debug::SetCrashKeyValue(kNumVariations,
      base::StringPrintf("%" PRIuS, variations.size()));

  std::string variations_string;
  variations_string.reserve(kHugeSize);

  for (size_t i = 0; i < variations.size(); ++i) {
    const std::string& variation = variations[i];
    // Do not truncate an individual experiment.
    if (variations_string.size() + variation.size() >= kHugeSize)
      break;
    variations_string += variation;
    variations_string += ",";
  }

  base::debug::SetCrashKeyValue(kVariations, variations_string);
}

void SetSwitchesFromCommandLine(const base::CommandLine& command_line,
                                SwitchFilterFunction skip_filter) {
  using SwitchesCrashKey = crash_reporter::CrashKeyString<64>;
  static SwitchesCrashKey switches_keys[] = {
      {"switch-1", SwitchesCrashKey::Tag::kArray},
      {"switch-2", SwitchesCrashKey::Tag::kArray},
      {"switch-3", SwitchesCrashKey::Tag::kArray},
      {"switch-4", SwitchesCrashKey::Tag::kArray},
      {"switch-5", SwitchesCrashKey::Tag::kArray},
      {"switch-6", SwitchesCrashKey::Tag::kArray},
      {"switch-7", SwitchesCrashKey::Tag::kArray},
      {"switch-8", SwitchesCrashKey::Tag::kArray},
      {"switch-9", SwitchesCrashKey::Tag::kArray},
      {"switch-10", SwitchesCrashKey::Tag::kArray},
      {"switch-11", SwitchesCrashKey::Tag::kArray},
      {"switch-12", SwitchesCrashKey::Tag::kArray},
      {"switch-13", SwitchesCrashKey::Tag::kArray},
      {"switch-14", SwitchesCrashKey::Tag::kArray},
      {"switch-15", SwitchesCrashKey::Tag::kArray},
  };

  const base::CommandLine::StringVector& argv = command_line.argv();

  // Set the number of switches in case size > kNumSwitches.
  static crash_reporter::CrashKeyString<4> num_switches_key("num-switches");
  num_switches_key.Set(base::NumberToString(argv.size() - 1));

  size_t key_i = 0;

  // Go through the argv, skipping the exec path. Stop if there are too many
  // switches to hold in crash keys.
  for (size_t i = 1; i < argv.size() && key_i < arraysize(switches_keys); ++i) {
#if defined(OS_WIN)
    std::string switch_str = base::WideToUTF8(argv[i]);
#else
    std::string switch_str = argv[i];
#endif

    // Skip uninteresting switches.
    if (skip_filter && (*skip_filter)(switch_str))
      continue;

    switches_keys[key_i++].Set(switch_str);
  }

  // Clear any remaining switches.
  for (; key_i < arraysize(switches_keys); ++key_i)
    switches_keys[key_i].Clear();
}

}  // namespace crash_keys
