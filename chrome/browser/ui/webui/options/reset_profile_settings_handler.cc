// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/reset_profile_settings_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kResetProfileSettingsLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=settings_reset_profile_settings";
}  // namespace

namespace options {

ResetProfileSettingsHandler::ResetProfileSettingsHandler() {
}

ResetProfileSettingsHandler::~ResetProfileSettingsHandler() {
}

void ResetProfileSettingsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  resetter_.reset(new ProfileResetter(profile));
}

void ResetProfileSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "resetProfileSettingsLabel", IDS_RESET_PROFILE_SETTINGS_LABEL },
    { "resetDefaultSearchEngineCheckbox",
       IDS_RESET_PROFILE_DEFAULT_SEARCH_ENGINE_CHECKBOX },
    { "resetHomepageCheckbox", IDS_RESET_PROFILE_HOMEPAGE_CHECKBOX },
    { "resetContentSettingsCheckbox",
       IDS_RESET_PROFILE_CONTENT_SETTINGS_CHECKBOX },
    { "resetCookiesAndSiteDataCheckbox", IDS_RESET_PROFILE_COOKIES_CHECKBOX },
    { "resetExtensionsCheckbox", IDS_RESET_PROFILE_EXTENSIONS_CHECKBOX },
    { "resetProfileSettingsCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "resetProfileSettingsOverlay",
                IDS_RESET_PROFILE_SETTINGS_TITLE);
  localized_strings->SetString(
      "resetProfileSettingsLearnMoreUrl",
      google_util::StringAppendGoogleLocaleParam(
          kResetProfileSettingsLearnMoreUrl));

  scoped_ptr<ListValue> reset_extensions_handling(new ListValue);
  for (int i = 0; i < 2; i++) {
    string16 label_string;
    switch (i) {
      case 0:
        label_string = l10n_util::GetStringUTF16(
            IDS_RESET_PROFILE_EXTENSIONS_DISABLE);
        break;
      case 1:
        label_string = l10n_util::GetStringUTF16(
            IDS_RESET_PROFILE_EXTENSIONS_UNINSTALL);
        break;
    }
    scoped_ptr<ListValue> option(new ListValue);
    option->Append(new base::FundamentalValue(i));
    option->Append(new base::StringValue(label_string));
    reset_extensions_handling->Append(option.release());
  }
  localized_strings->Set("resetExtensionsHandling",
                         reset_extensions_handling.release());
}

void ResetProfileSettingsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("performResetProfileSettings",
      base::Bind(&ResetProfileSettingsHandler::HandleResetProfileSettings,
                 base::Unretained(this)));
}

void ResetProfileSettingsHandler::HandleResetProfileSettings(
    const ListValue* /*value*/) {
  DCHECK(resetter_);
  DCHECK(!resetter_->IsActive());

  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();

  ProfileResetter::ResettableFlags reset_mask = 0;

  struct {
    const char* flag_name;
    ProfileResetter::Resettable mask;
  } name_to_flag[] = {
    { prefs::kResetDefaultSearchEngine,
      ProfileResetter::DEFAULT_SEARCH_ENGINE },
    { prefs::kResetHomepage, ProfileResetter::HOMEPAGE },
    { prefs::kResetContentSettings, ProfileResetter::CONTENT_SETTINGS },
    { prefs::kResetCookiesAndSiteData, ProfileResetter::COOKIES_AND_SITE_DATA },
    { prefs::kResetExtensions, ProfileResetter::EXTENSIONS },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(name_to_flag); ++i) {
    if (prefs->GetBoolean(name_to_flag[i].flag_name))
      reset_mask |= name_to_flag[i].mask;
  }

  ProfileResetter::ExtensionHandling extension_handling =
      (prefs->GetInteger(prefs::kResetExtensionsHandling) == 0)
          ? ProfileResetter::DISABLE_EXTENSIONS
          : ProfileResetter::UNINSTALL_EXTENSIONS;

  resetter_->Reset(
      reset_mask,
      extension_handling,
      base::Bind(&ResetProfileSettingsHandler::OnResetProfileSettingsDone,
                 AsWeakPtr()));
}

void ResetProfileSettingsHandler::OnResetProfileSettingsDone() {
  web_ui()->CallJavascriptFunction("ResetProfileSettingsOverlay.doneResetting");
}

}  // namespace options
