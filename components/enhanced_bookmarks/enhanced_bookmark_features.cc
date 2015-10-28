// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/enhanced_bookmark_features.h"

#include <string>

#include "base/command_line.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_switches.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/variations/variations_associated_data.h"

#if defined(OS_IOS) || defined(OS_ANDROID)

namespace enhanced_bookmarks {
namespace {
const char kFieldTrialName[] = "EnhancedBookmarks";
}  // namespace

bool IsEnhancedBookmarksEnabled() {
#if defined(OS_ANDROID)
  // If offline pages feature is enabled, also enable enhanced bookmarks feature
  // regardless its state.
  if (offline_pages::IsOfflinePagesEnabled())
    return true;
#endif

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

}  // namespace enhanced_bookmarks

#endif
