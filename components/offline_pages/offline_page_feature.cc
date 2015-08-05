// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/offline_page_feature.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/offline_pages/offline_page_switches.h"

#if defined(OS_ANDROID)

namespace offline_pages {

namespace {
const char kOfflinePagesFieldTrialName[] = "OfflinePages";
const char kOfflinePagesFieldTrialEnabledGroupName[] = "Enabled";
}  // namespace

bool IsOfflinePagesEnabled() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableOfflinePages)) {
    return true;
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOfflinePages)) {
    return false;
  }

  std::string group_name =
      base::FieldTrialList::FindFullName(kOfflinePagesFieldTrialName);
  return group_name == kOfflinePagesFieldTrialEnabledGroupName;
}

}  // namespace offline_pages

#endif
