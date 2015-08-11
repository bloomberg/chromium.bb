// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file can be empty. Its purpose is to contain the relatively short lived
// definitions required for experimental flags.

#include "ios/chrome/browser/experimental_flags.h"

#import <Foundation/Foundation.h>

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_features.h"
#include "components/variations/variations_associated_data.h"
#include "ios/chrome/browser/chrome_switches.h"
#include "ios/web/public/web_view_creation_util.h"

namespace {
NSString* const kEnableAlertOnBackgroundUpload =
    @"EnableAlertsOnBackgroundUpload";
NSString* const kEnableBookmarkRefreshImageOnEachVisit =
    @"EnableBookmarkRefreshImageOnEachVisit";
NSString* const kEnableViewCopyPasswords = @"EnableViewCopyPasswords";
}  // namespace

namespace experimental_flags {

bool IsAlertOnBackgroundUploadEnabled() {
  return [[NSUserDefaults standardUserDefaults]
      boolForKey:kEnableAlertOnBackgroundUpload];
}

bool IsExternalURLBlockingEnabled() {
  std::string group_name =
      base::FieldTrialList::FindFullName("IOSBlockUnpromptedExternalURLs");

  // Check if the experimental flag is turned on.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          switches::kEnableIOSBlockUnpromptedExternalURLs)) {
    return true;
  } else if (command_line->HasSwitch(
                 switches::kDisableIOSBlockUnpromptedExternalURLs)) {
    return false;
  }

  // Check if the finch experiment is turned on.
  return !base::StartsWith(group_name, "Disabled",
                           base::CompareCase::INSENSITIVE_ASCII);
}

bool IsBookmarkCollectionEnabled() {
  return enhanced_bookmarks::IsEnhancedBookmarksEnabled();
}

bool IsBookmarkImageFetchingOnVisitEnabled() {
  if (!IsBookmarkCollectionEnabled())
    return false;

  NSUserDefaults* user_defaults = [NSUserDefaults standardUserDefaults];
  if ([user_defaults boolForKey:kEnableBookmarkRefreshImageOnEachVisit])
    return true;

  const char kFieldTrialName[] = "EnhancedBookmarks";
  std::string enable_fetching = variations::GetVariationParamValue(
      kFieldTrialName, "EnableImagesFetchingOnVisit");
  return !enable_fetching.empty();
}

bool IsWKWebViewEnabled() {
  // If WKWebView isn't supported, don't activate the experiment at all. This
  // avoids someone being slotted into the WKWebView bucket (and thus reporting
  // as WKWebView), but actually running UIWebView.
  if (!web::IsWKWebViewSupported())
    return false;

  std::string group_name =
      base::FieldTrialList::FindFullName("IOSUseWKWebView");

  // First check if the experimental flag is turned on.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableIOSWKWebView)) {
    return true;
  } else if (command_line->HasSwitch(switches::kDisableIOSWKWebView)) {
    return false;
  }

  // Check if the finch experiment is turned on.
  return base::StartsWith(group_name, "Enabled",
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsViewCopyPasswordsEnabled() {
  NSString* viewCopyPasswordFlag = [[NSUserDefaults standardUserDefaults]
      objectForKey:kEnableViewCopyPasswords];
  if ([viewCopyPasswordFlag isEqualToString:@"Enabled"])
    return true;
  return false;
}

size_t MemoryWedgeSizeInMB() {
  std::string wedge_size_string;

  // Get the size from the Experimental setting.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  wedge_size_string =
      command_line->GetSwitchValueASCII(switches::kIOSMemoryWedgeSize);

  // Otherwise, get from a variation param.
  if (wedge_size_string.empty()) {
    wedge_size_string =
        variations::GetVariationParamValue("MemoryWedge", "wedge_size");
  }

  // Parse the value.
  size_t wedge_size_in_mb = 0;
  if (base::StringToSizeT(wedge_size_string, &wedge_size_in_mb))
    return wedge_size_in_mb;
  return 0;
}

}  // namespace experimental_flags
