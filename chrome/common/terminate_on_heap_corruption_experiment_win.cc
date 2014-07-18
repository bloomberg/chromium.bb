// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/terminate_on_heap_corruption_experiment_win.h"

#include "base/metrics/field_trial.h"
#include "base/win/registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"

#if defined(OS_WIN)
#if defined(GOOGLE_CHROME_BUILD)
#define PRODUCT_STRING_PATH L"Google\\Chrome"
#elif defined(CHROMIUM_BUILD)
#define PRODUCT_STRING_PATH L"Chromium"
#else
#error Unknown branding
#endif
#endif  // defined(OS_WIN)

namespace {

wchar_t* GetBeaconKeyPath() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
    return L"SOFTWARE\\" PRODUCT_STRING_PATH
        L"\\DisableTerminateOnProcessHeapCorruptionSxs";
  }
  return L"SOFTWARE\\" PRODUCT_STRING_PATH
      L"\\DisableTerminateOnProcessHeapCorruption";
}

}  // namespace

bool ShouldExperimentallyDisableTerminateOnHeapCorruption() {
  base::win::RegKey regkey(
      HKEY_CURRENT_USER, GetBeaconKeyPath(), KEY_QUERY_VALUE);
  return regkey.Valid();
}

void InitializeDisableTerminateOnHeapCorruptionExperiment() {
  base::win::RegKey regkey(HKEY_CURRENT_USER);

  if (base::FieldTrialList::FindFullName("TerminateOnProcessHeapCorruption") ==
      "Disabled") {
    regkey.CreateKey(GetBeaconKeyPath(), KEY_SET_VALUE);
  } else {
    regkey.DeleteKey(GetBeaconKeyPath());
  }
}
