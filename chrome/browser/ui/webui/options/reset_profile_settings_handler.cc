// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/reset_profile_settings_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter.h"
#include "chrome/browser/profile_resetter/automatic_profile_resetter_factory.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace options {

ResetProfileSettingsHandler::ResetProfileSettingsHandler()
    : automatic_profile_resetter_(NULL), has_shown_confirmation_dialog_(false) {
  google_util::GetBrand(&brandcode_);
}

ResetProfileSettingsHandler::~ResetProfileSettingsHandler() {}

void ResetProfileSettingsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  resetter_.reset(new ProfileResetter(profile));
  automatic_profile_resetter_ =
      AutomaticProfileResetterFactory::GetForBrowserContext(profile);
  DCHECK(automatic_profile_resetter_);
}

void ResetProfileSettingsHandler::InitializePage() {
  web_ui()->CallJavascriptFunction(
      "ResetProfileSettingsOverlay.setResettingState",
      base::FundamentalValue(resetter_->IsActive()));
}

void ResetProfileSettingsHandler::Uninitialize() {
  if (has_shown_confirmation_dialog_) {
    DCHECK(automatic_profile_resetter_);
    automatic_profile_resetter_->NotifyDidCloseWebUIResetDialog(
        false /*performed_reset*/);
  }
}

void ResetProfileSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "resetProfileSettingsCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON },
    { "resetProfileSettingsExplanation",
        IDS_RESET_PROFILE_SETTINGS_EXPLANATION},
    { "resetProfileSettingsFeedback", IDS_RESET_PROFILE_SETTINGS_FEEDBACK }
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "resetProfileSettingsOverlay",
                IDS_RESET_PROFILE_SETTINGS_TITLE);
  localized_strings->SetString(
      "resetProfileSettingsLearnMoreUrl",
      chrome::kResetProfileSettingsLearnMoreURL);
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
    const ListValue* value) {
  bool send_settings = false;
  if (!value->GetBoolean(0, &send_settings))
    NOTREACHED();

  DCHECK(brandcode_.empty() || config_fetcher_);
  if (config_fetcher_ && config_fetcher_->IsActive()) {
    // Reset once the prefs are fetched.
    config_fetcher_->SetCallback(
        base::Bind(&ResetProfileSettingsHandler::ResetProfile,
                   Unretained(this),
                   send_settings));
  } else {
    ResetProfile(send_settings);
  }
}

void ResetProfileSettingsHandler::OnResetProfileSettingsDone() {
  DCHECK(automatic_profile_resetter_);
  web_ui()->CallJavascriptFunction("ResetProfileSettingsOverlay.doneResetting");
  if (setting_snapshot_) {
    Profile* profile = Profile::FromWebUI(web_ui());
    ResettableSettingsSnapshot current_snapshot(profile);
    int difference = setting_snapshot_->FindDifferentFields(current_snapshot);
    if (difference) {
      setting_snapshot_->Subtract(current_snapshot);
      std::string report = SerializeSettingsReport(*setting_snapshot_,
                                                   difference);
      bool is_reset_prompt_active =
          automatic_profile_resetter_->IsResetPromptFlowActive();
      SendSettingsFeedback(report, profile, is_reset_prompt_active ?
          PROFILE_RESET_PROMPT : PROFILE_RESET_WEBUI);
    }
    setting_snapshot_.reset();
  }
  automatic_profile_resetter_->NotifyDidCloseWebUIResetDialog(
      true /*performed_reset*/);
}

void ResetProfileSettingsHandler::OnShowResetProfileDialog(const ListValue*) {
  DictionaryValue flashInfo;
  flashInfo.Set("feedbackInfo", GetReadableFeedback(
      Profile::FromWebUI(web_ui())));
  web_ui()->CallJavascriptFunction(
      "ResetProfileSettingsOverlay.setFeedbackInfo",
      flashInfo);

  automatic_profile_resetter_->NotifyDidOpenWebUIResetDialog();
  has_shown_confirmation_dialog_ = true;

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

void ResetProfileSettingsHandler::ResetProfile(bool send_settings) {
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
  // Save current settings if required.
  setting_snapshot_.reset(send_settings ?
      new ResettableSettingsSnapshot(Profile::FromWebUI(web_ui())) : NULL);
  resetter_->Reset(
      ProfileResetter::ALL,
      default_settings.Pass(),
      base::Bind(&ResetProfileSettingsHandler::OnResetProfileSettingsDone,
                 AsWeakPtr()));
  content::RecordAction(content::UserMetricsAction("ResetProfile"));
  UMA_HISTOGRAM_BOOLEAN("ProfileReset.SendFeedback", send_settings);
}

}  // namespace options
