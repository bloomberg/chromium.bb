// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/srt_client_info_win.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/channel_info.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"
#include "components/version_info/version_info.h"

namespace safe_browsing {

namespace {

bool SafeBrowsingExtendedEnabledForBrowser(const Browser* browser) {
  const Profile* profile = browser->profile();
  return profile && !profile->IsOffTheRecord() &&
         IsExtendedReportingEnabled(*profile->GetPrefs());
}

}  // namespace

const char kChromeChannelSwitch[] = "chrome-channel";
const char kChromeVersionSwitch[] = "chrome-version";
const char kEnableCrashReporting[] = "enable-crash-reporting";
const char kExtendedSafeBrowsingEnabledSwitch[] =
    "extended-safebrowsing-enabled";

int ChannelAsInt() {
  switch (chrome::GetChannel()) {
    case version_info::Channel::UNKNOWN:
      return 0;
    case version_info::Channel::CANARY:
      return 1;
    case version_info::Channel::DEV:
      return 2;
    case version_info::Channel::BETA:
      return 3;
    case version_info::Channel::STABLE:
      return 4;
  }
  NOTREACHED();
  return 0;
}

bool SafeBrowsingExtendedReportingEnabled() {
  BrowserList* browser_list = BrowserList::GetInstance();
  return std::any_of(browser_list->begin_last_active(),
                     browser_list->end_last_active(),
                     &SafeBrowsingExtendedEnabledForBrowser);
}

}  // namespace safe_browsing
