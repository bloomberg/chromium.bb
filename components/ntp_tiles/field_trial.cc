// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_tiles/field_trial.h"

#if defined(OS_ANDROID)
#include <jni.h>
#endif
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "components/ntp_tiles/constants.h"
#include "components/ntp_tiles/switches.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/MostVisitedSites_jni.h"
#endif

namespace ntp_tiles {

bool ShouldShowPopularSites() {
  // Note: It's important to query the field trial state first, to ensure that
  // UMA reports the correct group.
  const std::string group_name =
      base::FieldTrialList::FindFullName(kPopularSitesFieldTrialName);

  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (cmd_line->HasSwitch(switches::kDisableNTPPopularSites))
    return false;

  if (cmd_line->HasSwitch(switches::kEnableNTPPopularSites))
    return true;

#if defined(OS_ANDROID)
  if (Java_MostVisitedSites_isPopularSitesForceEnabled(
          base::android::AttachCurrentThread())) {
    return true;
  }
#endif

  // Until any configuration is fetched, the default is to enable popular sites.
  if (group_name.empty()) {
    return true;
  }

  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

}  // namespace ntp_tiles
