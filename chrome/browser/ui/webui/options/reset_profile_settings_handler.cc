// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/reset_profile_settings_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const char kResetProfileSettingsLearnMoreUrl[] =
    "https://support.google.com/chrome/?p=ui_reset_settings";
}  // namespace

namespace options {

ResetProfileSettingsHandler::ResetProfileSettingsHandler() {
  google_util::GetBrand(&brandcode_);
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
    { "resetProfileSettingsCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON },
    { "resetProfileSettingsExplanation", IDS_RESET_PROFILE_SETTINGS_EXPLANATION}
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "resetProfileSettingsOverlay",
                IDS_RESET_PROFILE_SETTINGS_TITLE);
  localized_strings->SetString(
      "resetProfileSettingsLearnMoreUrl",
      google_util::StringAppendGoogleLocaleParam(
          kResetProfileSettingsLearnMoreUrl));
}

void ResetProfileSettingsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("performResetProfileSettings",
      base::Bind(&ResetProfileSettingsHandler::HandleResetProfileSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onShowResetProfileDialog",
      base::Bind(&ResetProfileSettingsHandler::OnShowResetProfileDialog,
                 base::Unretained(this)));
}

void ResetProfileSettingsHandler::HandleResetProfileSettings(
    const ListValue* /*value*/) {
  DCHECK(brandcode_.empty() || config_fetcher_);
  if (config_fetcher_ && config_fetcher_->IsActive()) {
    // Reset once the prefs are fetched.
    config_fetcher_->SetCallback(
        base::Bind(&ResetProfileSettingsHandler::ResetProfile,
                   Unretained(this)));
  } else {
    ResetProfile();
  }
}

void ResetProfileSettingsHandler::OnResetProfileSettingsDone() {
  web_ui()->CallJavascriptFunction("ResetProfileSettingsOverlay.doneResetting");
}

void ResetProfileSettingsHandler::OnShowResetProfileDialog(const ListValue*) {
  if (brandcode_.empty())
    return;
  config_fetcher_.reset(new BrandcodeConfigFetcher(
      base::Bind(&ResetProfileSettingsHandler::OnSettingsFetched,
                 Unretained(this)),
      GURL("https://tools.google.com/service/update2"),
      brandcode_));
}

void ResetProfileSettingsHandler::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());
  // The master prefs is fetched. We are waiting for user pressing 'Reset'.
}

void ResetProfileSettingsHandler::ResetProfile() {
  DCHECK(resetter_);
  DCHECK(!resetter_->IsActive());

  scoped_ptr<BrandcodedDefaultSettings> default_settings;
  if (config_fetcher_) {
    DCHECK(!config_fetcher_->IsActive());
    default_settings = config_fetcher_->GetSettings();
    config_fetcher_.reset();
  } else {
    DCHECK(brandcode_.empty());
  }

  // If failed to fetch BrandcodedDefaultSettings or this is an organic
  // installation, use default settings.
  if (!default_settings)
    default_settings.reset(new BrandcodedDefaultSettings);
  resetter_->Reset(
      ProfileResetter::ALL,
      default_settings.Pass(),
      base::Bind(&ResetProfileSettingsHandler::OnResetProfileSettingsDone,
                 AsWeakPtr()));
  content::RecordAction(content::UserMetricsAction("ResetProfile"));
}

}  // namespace options
