// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/reset_settings_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/browser/profile_resetter/brandcode_config_fetcher.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/profile_reset_report.pb.h"
#include "chrome/browser/profile_resetter/profile_resetter.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/reset/metrics.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
#include "chrome/browser/profile_resetter/triggered_profile_resetter.h"
#include "chrome/browser/profile_resetter/triggered_profile_resetter_factory.h"
#endif  // defined(OS_WIN)

namespace settings {

ResetSettingsHandler::ResetSettingsHandler(
    Profile* profile, bool allow_powerwash)
    : profile_(profile), weak_ptr_factory_(this) {
#if defined(OS_CHROMEOS)
  allow_powerwash_ = allow_powerwash;
#endif  // defined(OS_CHROMEOS)
  google_brand::GetBrand(&brandcode_);
}

ResetSettingsHandler::~ResetSettingsHandler() {}

ResetSettingsHandler* ResetSettingsHandler::Create(
    content::WebUIDataSource* html_source, Profile* profile) {
  bool allow_powerwash = false;
#if defined(OS_CHROMEOS)
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  allow_powerwash = !connector->IsEnterpriseManaged() &&
      !user_manager::UserManager::Get()->IsLoggedInAsGuest() &&
      !user_manager::UserManager::Get()->IsLoggedInAsSupervisedUser();
  html_source->AddBoolean("allowPowerwash", allow_powerwash);
#endif  // defined(OS_CHROMEOS)

  bool show_reset_profile_banner = false;
  static const int kBannerShowTimeInDays = 5;
  const base::Time then = chrome_prefs::GetResetTime(profile);
  if (!then.is_null()) {
    show_reset_profile_banner =
        (base::Time::Now() - then).InDays() < kBannerShowTimeInDays;
  }
  html_source->AddBoolean("showResetProfileBanner", show_reset_profile_banner);

  // Inject |allow_powerwash| for testing.
  return new ResetSettingsHandler(profile, allow_powerwash);
}

void ResetSettingsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("performResetProfileSettings",
      base::Bind(&ResetSettingsHandler::HandleResetProfileSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onShowResetProfileDialog",
      base::Bind(&ResetSettingsHandler::OnShowResetProfileDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("getReportedSettings",
      base::Bind(&ResetSettingsHandler::HandleGetReportedSettings,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onHideResetProfileDialog",
      base::Bind(&ResetSettingsHandler::OnHideResetProfileDialog,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("onHideResetProfileBanner",
      base::Bind(&ResetSettingsHandler::OnHideResetProfileBanner,
                 base::Unretained(this)));
#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
       "onPowerwashDialogShow",
       base::Bind(&ResetSettingsHandler::OnShowPowerwashDialog,
                  base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "requestFactoryResetRestart",
      base::Bind(&ResetSettingsHandler::HandleFactoryResetRestart,
                 base::Unretained(this)));
#endif  // defined(OS_CHROMEOS)
}

void ResetSettingsHandler::HandleResetProfileSettings(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));
  bool send_settings = false;
  CHECK(args->GetBoolean(1, &send_settings));

  DCHECK(brandcode_.empty() || config_fetcher_);
  if (config_fetcher_ && config_fetcher_->IsActive()) {
    // Reset once the prefs are fetched.
    config_fetcher_->SetCallback(
        base::Bind(&ResetSettingsHandler::ResetProfile,
                   base::Unretained(this),
                   callback_id,
                   send_settings));
  } else {
    ResetProfile(callback_id, send_settings);
  }
}

void ResetSettingsHandler::OnResetProfileSettingsDone(
    std::string callback_id, bool send_feedback) {
  ResolveJavascriptCallback(
      base::StringValue(callback_id), *base::Value::CreateNullValue());
  if (send_feedback && setting_snapshot_) {
    ResettableSettingsSnapshot current_snapshot(profile_);
    int difference = setting_snapshot_->FindDifferentFields(current_snapshot);
    if (difference) {
      setting_snapshot_->Subtract(current_snapshot);
      std::unique_ptr<reset_report::ChromeResetReport> report_proto =
          SerializeSettingsReportToProto(*setting_snapshot_, difference);
      if (report_proto)
        SendSettingsFeedbackProto(*report_proto, profile_);
    }
  }
  setting_snapshot_.reset();
}

void ResetSettingsHandler::HandleGetReportedSettings(
    const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  std::string callback_id;
  CHECK(args->GetString(0, &callback_id));

  setting_snapshot_->RequestShortcuts(base::Bind(
      &ResetSettingsHandler::OnGetReportedSettingsDone,
      weak_ptr_factory_.GetWeakPtr(),
      callback_id));
}

void ResetSettingsHandler::OnGetReportedSettingsDone(std::string callback_id) {
  std::unique_ptr<base::ListValue> list =
      GetReadableFeedbackForSnapshot(profile_, *setting_snapshot_);
  ResolveJavascriptCallback(base::StringValue(callback_id), *list);
}

void ResetSettingsHandler::OnShowResetProfileDialog(
    const base::ListValue* args) {
  if (!GetResetter()->IsActive()) {
    setting_snapshot_.reset(new ResettableSettingsSnapshot(profile_));
  }

  if (brandcode_.empty())
    return;
  config_fetcher_.reset(new BrandcodeConfigFetcher(
      base::Bind(&ResetSettingsHandler::OnSettingsFetched,
                 base::Unretained(this)),
      GURL("https://tools.google.com/service/update2"),
      brandcode_));
}

void ResetSettingsHandler::OnHideResetProfileDialog(
    const base::ListValue* args) {
  if (!GetResetter()->IsActive())
    setting_snapshot_.reset();
}

void ResetSettingsHandler::OnHideResetProfileBanner(
    const base::ListValue* args) {
  chrome_prefs::ClearResetTime(profile_);
}

void ResetSettingsHandler::OnSettingsFetched() {
  DCHECK(config_fetcher_);
  DCHECK(!config_fetcher_->IsActive());
  // The master prefs is fetched. We are waiting for user pressing 'Reset'.
}

void ResetSettingsHandler::ResetProfile(std::string callback_id,
                                        bool send_settings) {
  DCHECK(!GetResetter()->IsActive());

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

  GetResetter()->Reset(
      ProfileResetter::ALL, std::move(default_settings),
      base::Bind(&ResetSettingsHandler::OnResetProfileSettingsDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback_id,
                 send_settings));
  content::RecordAction(base::UserMetricsAction("ResetProfile"));
  UMA_HISTOGRAM_BOOLEAN("ProfileReset.SendFeedback", send_settings);
}

ProfileResetter* ResetSettingsHandler::GetResetter() {
  if (!resetter_)
    resetter_.reset(new ProfileResetter(profile_));
  return resetter_.get();
}

#if defined(OS_CHROMEOS)
void ResetSettingsHandler::OnShowPowerwashDialog(
     const base::ListValue* args) {
  UMA_HISTOGRAM_ENUMERATION(
      "Reset.ChromeOS.PowerwashDialogShown",
      chromeos::reset::DIALOG_FROM_OPTIONS,
      chromeos::reset::DIALOG_VIEW_TYPE_SIZE);
}

void ResetSettingsHandler::HandleFactoryResetRestart(
    const base::ListValue* args) {
  if (!allow_powerwash_)
    return;

  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kFactoryResetRequested, true);
  prefs->CommitPendingWrite();

  // Perform sign out. Current chrome process will then terminate, new one will
  // be launched (as if it was a restart).
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace settings
