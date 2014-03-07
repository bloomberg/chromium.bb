// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/automatic_settings_reset_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace {

void OnDismissedAutomaticSettingsResetBanner(Profile* profile,
                                             const base::ListValue* value) {
  chrome_prefs::ClearResetTime(profile);
}

}  // namespace

namespace options {

AutomaticSettingsResetHandler::AutomaticSettingsResetHandler() {}
AutomaticSettingsResetHandler::~AutomaticSettingsResetHandler() {}

void AutomaticSettingsResetHandler::InitializePage() {
  static const int kBannerShowTimeInDays = 5;

  const base::Time then =
      chrome_prefs::GetResetTime(Profile::FromWebUI(web_ui()));
  if (!then.is_null()) {
    const base::Time now = base::Time::Now();
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
  web_ui()->RegisterMessageCallback(
      "onDismissedAutomaticSettingsResetBanner",
      base::Bind(&OnDismissedAutomaticSettingsResetBanner,
                 Profile::FromWebUI(web_ui())));
}

}  // namespace options
