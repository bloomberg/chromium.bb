// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_util.h"

#include "base/command_line.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/common/content_switches.h"

namespace web_intents {

void RegisterUserPrefs(PrefService* user_prefs) {
  user_prefs->RegisterBooleanPref(prefs::kWebIntentsEnabled, true,
                                  PrefService::SYNCABLE_PREF);
}

bool IsWebIntentsEnabled(Profile* profile) {
  bool disabled_flag = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableWebIntents);

  bool enabled_pref = profile->GetPrefs()->GetBoolean(
      prefs::kWebIntentsEnabled);

  return !disabled_flag && enabled_pref;
}

bool IsWebIntentsEnabledInActiveBrowser() {
  Browser* browser = BrowserList::GetLastActive();
  if (!browser)
    browser = *BrowserList::begin();
  DCHECK(browser);

  return IsWebIntentsEnabled(browser->profile());
}

}  // namespace web_intents
