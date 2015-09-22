// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/core/common/crash_keys.h"

#include "base/debug/crash_logging.h"
#include "base/format_macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace crash_keys {

#if defined(OS_MACOSX)
// Crashpad owns the "guid" key. Chrome's metrics client ID is a separate ID
// carried in a distinct "metrics_client_id" field.
const char kMetricsClientId[] = "metrics_client_id";
#else
const char kClientId[] = "guid";
#endif

const char kChannel[] = "channel";

const char kNumVariations[] = "num-experiments";
const char kVariations[] = "variations";

const char kBug464926CrashKey[] = "bug-464926-info";

#if defined(OS_MACOSX)
namespace mac {

const char kZombie[] = "zombie";
const char kZombieTrace[] = "zombie_dealloc_bt";

}  // namespace mac
#endif

void SetMetricsClientIdFromGUID(const std::string& metrics_client_guid) {
  std::string stripped_guid(metrics_client_guid);
  // Remove all instance of '-' char from the GUID. So BCD-WXY becomes BCDWXY.
  base::ReplaceSubstringsAfterOffset(
      &stripped_guid, 0, "-", base::StringPiece());
  if (stripped_guid.empty())
    return;

#if defined(OS_MACOSX)
  // The crash client ID is maintained by Crashpad and is distinct from the
  // metrics client ID, which is carried in its own key.
  base::debug::SetCrashKeyValue(kMetricsClientId, stripped_guid);
#else
  // The crash client ID is set by the application when Breakpad is in use.
  // The same ID as the metrics client ID is used.
  base::debug::SetCrashKeyValue(kClientId, stripped_guid);
#endif
}

void ClearMetricsClientId() {
#if defined(OS_MACOSX)
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
  base::debug::ClearCrashKey(kMetricsClientId);
#endif
}

void SetVariationsList(const std::vector<std::string>& variations) {
  base::debug::SetCrashKeyValue(kNumVariations,
      base::StringPrintf("%" PRIuS, variations.size()));

  std::string variations_string;
  variations_string.reserve(kLargeSize);

  for (size_t i = 0; i < variations.size(); ++i) {
    const std::string& variation = variations[i];
    // Do not truncate an individual experiment.
    if (variations_string.size() + variation.size() >= kLargeSize)
      break;
    variations_string += variation;
    variations_string += ",";
  }

  base::debug::SetCrashKeyValue(kVariations, variations_string);
}

}  // namespace crash_keys
