// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/reset_profile_settings_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#endif  // defined(OS_WIN)

namespace options {

namespace {

reset_report::ChromeResetReport::ResetRequestOrigin
ResetRequestOriginFromString(const std::string& reset_request_origin) {
  static const char kOriginUserClick[] = "userclick";
  static const char kOriginTriggeredReset[] = "triggeredreset";

  if (reset_request_origin ==
      ResetProfileSettingsHandler::kCctResetSettingsHash) {
    return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_CCT;
  }
  if (reset_request_origin == kOriginUserClick)
    return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_USER_CLICK;
  if (reset_request_origin == kOriginTriggeredReset) {
    return reset_report::ChromeResetReport::
        RESET_REQUEST_ORIGIN_TRIGGERED_RESET;
  }
  if (!reset_request_origin.empty())
    NOTREACHED();

  return reset_report::ChromeResetReport::RESET_REQUEST_ORIGIN_UNKNOWN;
}

}  // namespace

const char ResetProfileSettingsHandler::kCctResetSettingsHash[] = "cct";

ResetProfileSettingsHandler::ResetProfileSettingsHandler() {
  google_brand::GetBrand(&brandcode_);
}

ResetProfileSettingsHandler::~ResetProfileSettingsHandler() {}

void ResetProfileSettingsHandler::InitializeHandler() {
  Profile* profile = Profile::FromWebUI(web_ui());
  resetter_.reset(new ProfileResetter(profile));
}

void ResetProfileSettingsHandler::InitializePage() {
  web_ui()->CallJavascriptFunctionUnsafe(
      "ResetProfileSettingsOverlay.setResettingState",
      base::Value(resetter_->IsActive()));
}

void ResetProfileSettingsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "resetProfileSettingsCommit", IDS_RESET_PROFILE_SETTINGS_COMMIT_BUTTON },
    { "resetProfileSettingsExplanation",
        IDS_RESET_PROFILE_SETTINGS_EXPLANATION },
    { "resetProfileSettingsFeedback", IDS_RESET_PROFILE_SETTINGS_FEEDBACK }
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "resetProfileSettingsOverlay",
                IDS_RESET_PROFILE_SETTINGS_TITLE);
  localized_strings->SetString(
      "resetProfileSettingsLearnMoreUrl",
      chrome::kResetProfileSettingsLearnMoreURL);

  // Set up the localized strings for the triggered profile reset overlay.
  // The reset tool name can currently only have a custom value on Windows.
  base::string16 reset_tool_name;
#if defined(OS_WIN)
  Profile* profile = Profile::FromWebUI(web_ui());
  TriggeredProfileResetter* triggered_profile_resetter =
      TriggeredProfileResetterFactory::GetForBrowserContext(profile);
  // TriggeredProfileResetter instance will be nullptr for incognito profiles.
  if (triggered_profile_resetter) {
    reset_tool_name = triggered_profile_resetter->GetResetToolName();

    // Now that a reset UI has been shown, don't trigger again for this profile.
    triggered_profile_resetter->ClearResetTrigger();
  }
#endif

  if (reset_tool_name.empty()) {
    reset_tool_name = l10n_util::GetStringUTF16(
        IDS_TRIGGERED_RESET_PROFILE_SETTINGS_DEFAULT_TOOL_NAME);
  }
  localized_strings->SetString(
      "triggeredResetProfileSettingsOverlay",
      l10n_util::GetStringFUTF16(IDS_TRIGGERED_RESET_PROFILE_SETTINGS_TITLE,
                                 reset_tool_name));
  // Set the title manually since RegisterTitle() wants an id.
  base::string16 title_string(l10n_util::GetStringFUTF16(
      IDS_TRIGGERED_RESET_PROFILE_SETTINGS_TITLE, reset_tool_name));
  localized_strings->SetString("triggeredResetProfileSettingsOverlay",
                               title_string);
  localized_strings->SetString(
      "triggeredResetProfileSettingsOverlayTabTitle",
      l10n_util::GetStringFUTF16(IDS_OPTIONS_TAB_TITLE,
                                 l10n_util::GetStringUTF16(IDS_SETTINGS_TITLE),
                                 title_string));
  localized_strings->SetString(
      "triggeredResetProfileSettingsExplanation",
      l10n_util::GetStringFUTF16(
          IDS_TRIGGERED_RESET_PROFILE_SETTINGS_EXPLANATION, reset_tool_name));
}

void ResetProfileSettingsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("performResetProfileSettings",
      base::Bind(&ResetProfileSettingsHandler::HandleResetProfileSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onShowResetProfileDialog",
      base::Bind(&ResetProfileSettingsHandler::OnShowResetProfileDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onHideResetProfileDialog",
      base::Bind(&ResetProfileSettingsHandler::OnHideResetProfileDialog,
                 base::Unretained(this)));
}

void ResetProfileSettingsHandler::HandleResetProfileSettings(
    const base::ListValue* value) {
  bool send_settings = false;
  std::string request_origin_string;
  bool success = value->GetBoolean(0, &send_settings) &&
                 value->GetString(1, &request_origin_string);
  DCHECK(success);

  DCHECK(brandcode_.empty() || config_fetcher_);
  reset_report::ChromeResetReport::ResetRequestOrigin request_origin =
      ResetRequestOriginFromString(request_origin_string);
  if (config_fetcher_ && config_fetcher_->IsActive()) {
    // Reset once the prefs are fetched.
    config_fetcher_->SetCallback(
        base::Bind(&ResetProfileSettingsHandler::ResetProfile, Unretained(this),
                   send_settings, request_origin));
  } else {
    ResetProfile(send_settings, request_origin);
  }
}

void ResetProfileSettingsHandler::OnResetProfileSettingsDone(
    bool send_feedback,
    reset_report::ChromeResetReport::ResetRequestOrigin request_origin) {
  web_ui()->CallJavascriptFunctionUnsafe(
      "ResetProfileSettingsOverlay.doneResetting");

  if (send_feedback && setting_snapshot_) {
    Profile* profile = Profile::FromWebUI(web_ui());
    ResettableSettingsSnapshot current_snapshot(profile);
    int difference = setting_snapshot_->FindDifferentFields(current_snapshot);
    if (difference) {
      setting_snapshot_->Subtract(current_snapshot);
      std::unique_ptr<reset_report::ChromeResetReport> report_proto =
          SerializeSettingsReportToProto(*setting_snapshot_, difference);
      if (report_proto) {
        report_proto->set_reset_request_origin(request_origin);
        SendSettingsFeedbackProto(*report_proto, profile);
      }
    }
  }
  setting_snapshot_.reset();
}

void ResetProfileSettingsHandler::OnShowResetProfileDialog(
    const base::ListValue* value) {
  if (!resetter_->IsActive()) {
    setting_snapshot_.reset(
        new ResettableSettingsSnapshot(Profile::FromWebUI(web_ui())));
    setting_snapshot_->RequestShortcuts(base::Bind(
        &ResetProfileSettingsHandler::UpdateFeedbackUI, AsWeakPtr()));
    UpdateFeedbackUI();
  }

  if (brandcode_.empty())
    return;
  config_fetcher_.reset(new BrandcodeConfigFetcher(
      base::Bind(&ResetProfileSettingsHandler::OnSettingsFetched,
                 Unretained(this)),
      GURL("https://tools.google.com/service/update2"),
      brandcode_));
}

void ResetProfileSettingsHandler::OnHideResetProfileDialog(
    const base::ListValue* value) {
  if (!resetter_->IsActive())
    setting_snapshot_.reset();
}

void ResetProfileSettingsHandler::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());
  // The master prefs is fetched. We are waiting for user pressing 'Reset'.
}

void ResetProfileSettingsHandler::ResetProfile(
    bool send_settings,
    reset_report::ChromeResetReport::ResetRequestOrigin request_origin) {
  DCHECK(resetter_);
  DCHECK(!resetter_->IsActive());

  std::unique_ptr<BrandcodedDefaultSettings> default_settings;
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
      ProfileResetter::ALL, std::move(default_settings),
      base::Bind(&ResetProfileSettingsHandler::OnResetProfileSettingsDone,
                 AsWeakPtr(), send_settings, request_origin));
  content::RecordAction(base::UserMetricsAction("ResetProfile"));
  UMA_HISTOGRAM_BOOLEAN("ProfileReset.SendFeedback", send_settings);
  UMA_HISTOGRAM_ENUMERATION(
      "ProfileReset.ResetRequestOrigin", request_origin,
      reset_report::ChromeResetReport::ResetRequestOrigin_MAX + 1);
}

void ResetProfileSettingsHandler::UpdateFeedbackUI() {
  if (!setting_snapshot_)
    return;
  std::unique_ptr<base::ListValue> list = GetReadableFeedbackForSnapshot(
      Profile::FromWebUI(web_ui()), *setting_snapshot_);
  base::DictionaryValue feedback_info;
  feedback_info.Set("feedbackInfo", list.release());
  web_ui()->CallJavascriptFunctionUnsafe(
      "ResetProfileSettingsOverlay.setFeedbackInfo", feedback_info);
}

}  // namespace options
