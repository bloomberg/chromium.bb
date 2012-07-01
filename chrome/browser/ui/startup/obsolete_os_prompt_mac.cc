// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_prompt.h"

#include "base/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/cocoa/obsolete_os.h"
#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"

namespace chrome {

void RegisterObsoleteOSInfobarPrefs(PrefService* local_state) {
  local_state->RegisterDoublePref(
      prefs::kMacLeopardObsoleteInfobarLastShown,
      0,
      PrefService::UNSYNCABLE_PREF);
}

void ShowObsoleteOSPrompt(Browser* browser) {
  if (!IsOSObsoleteOrNearlySo())
    return;

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state)
    return;

  // Only show the infobar if the user has not been shown it for more than a
  // week.
  base::Time time_now(base::Time::Now());
  if (local_state->HasPrefPath(prefs::kMacLeopardObsoleteInfobarLastShown)) {
    double time_double =
        local_state->GetDouble(prefs::kMacLeopardObsoleteInfobarLastShown);
    base::Time last_shown(base::Time::FromDoubleT(time_double));

    base::TimeDelta a_week(base::TimeDelta::FromDays(7));
    if (last_shown >= time_now - a_week)
      return;
  }

  TabContents* tab = chrome::GetActiveTabContents(browser);
  if (!tab)
    return;
  tab->infobar_tab_helper()->AddInfoBar(
      new ObsoleteOSInfoBar(
          tab->infobar_tab_helper(),
          LocalizedObsoleteOSString(),
          GURL(chrome::kMacLeopardObsoleteURL)));

  local_state->SetDouble(prefs::kMacLeopardObsoleteInfobarLastShown,
      time_now.ToDoubleT());
}

}  // namespace chrome
