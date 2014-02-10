// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/automatic_settings_reset_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace {

const int kBannerShowTimeInDays = 5;

// Called when the user dismisses the automatic settings reset banner.
void OnDismissedAutomaticSettingsResetBanner(const base::ListValue* value) {
  PrefService* local_state = g_browser_process->local_state();
  local_state->ClearPref(prefs::kProfilePreferenceResetTime);
}

}  // namespace

namespace options {

AutomaticSettingsResetHandler::AutomaticSettingsResetHandler() {}
AutomaticSettingsResetHandler::~AutomaticSettingsResetHandler() {}

void AutomaticSettingsResetHandler::InitializePage() {
  const int64 reset_time =
      g_browser_process->local_state()->GetInt64(
          prefs::kProfilePreferenceResetTime);
  if (reset_time != 0) {
    const base::Time now = base::Time::Now();
    const base::Time then = base::Time::FromInternalValue(reset_time);
    if ((now - then).InDays() < kBannerShowTimeInDays)
      web_ui()->CallJavascriptFunction("AutomaticSettingsResetBanner.show");
  }
}

void AutomaticSettingsResetHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static const OptionsStringResource resources[] = {
      { "automaticSettingsResetBannerText",
        IDS_AUTOMATIC_SETTINGS_RESET_BANNER_TEXT },
      { "automaticSettingsResetLearnMoreUrl",
        IDS_LEARN_MORE },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  localized_strings->SetString(
      "automaticSettingsResetLearnMoreUrl",
      chrome::kAutomaticSettingsResetLearnMoreURL);
}

void AutomaticSettingsResetHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("onDismissedAutomaticSettingsResetBanner",
      base::Bind(&OnDismissedAutomaticSettingsResetBanner));
}

}  // namespace options
