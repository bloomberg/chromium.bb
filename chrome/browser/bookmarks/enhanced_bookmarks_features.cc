// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/enhanced_bookmarks_features.h"

#include <string>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "components/variations/variations_associated_data.h"

namespace {

const char kFieldTrialName[] = "EnhancedBookmarks";

}  // namespace

bool IsEnhancedBookmarksEnabled() {
  // Enhanced bookmarks is not used on desktop, so it shouldn't be calling this
  // function.
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  NOTREACHED();
#endif  // !defined(OS_IOS) || !defined(OS_ANDROID)

  // kEnhancedBookmarksExperiment flag could have values "", "1" and "0".  "" -
  // default, "0" - user opted out, "1" - user opted in.  Tests also use the
  // command line flag to force enhanced bookmark to be on.
  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kEnhancedBookmarksExperiment);
  if (switch_value == "1")
    return true;
  if (switch_value == "0")
    return false;

  // Check that the "id" param is present. This is a legacy of the desktop
  // implementation providing the extension id via param. This probably should
  // be replaced with code that checks the experiment name instead.
  return !variations::GetVariationParamValue(kFieldTrialName, "id").empty();
}

bool IsEnableDomDistillerSet() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDomDistiller)) {
    return true;
  }
  if (variations::GetVariationParamValue(kFieldTrialName,
                                         "enable-dom-distiller") == "1")
    return true;

  return false;
}

bool IsEnableSyncArticlesSet() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSyncArticles)) {
    return true;
  }
  if (variations::GetVariationParamValue(kFieldTrialName,
                                         "enable-sync-articles") == "1")
    return true;

  return false;
}
