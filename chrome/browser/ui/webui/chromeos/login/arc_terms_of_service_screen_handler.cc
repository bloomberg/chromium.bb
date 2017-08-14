// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"

#include "base/i18n/timezone.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen_view_observer.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kJsScreenPath[] = "login.ArcTermsOfServiceScreen";

}  // namespace

namespace chromeos {

ArcTermsOfServiceScreenHandler::ArcTermsOfServiceScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_call_js_prefix(kJsScreenPath);
}

ArcTermsOfServiceScreenHandler::~ArcTermsOfServiceScreenHandler() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  for (auto& observer : observer_list_)
    observer.OnViewDestroyed(this);
}

void ArcTermsOfServiceScreenHandler::RegisterMessages() {
  AddCallback("arcTermsOfServiceSkip",
              &ArcTermsOfServiceScreenHandler::HandleSkip);
  AddCallback("arcTermsOfServiceAccept",
              &ArcTermsOfServiceScreenHandler::HandleAccept);
}

void ArcTermsOfServiceScreenHandler::UpdateTimeZone() {
  const std::string country_code = base::CountryCodeForCurrentTimezone();
  if (country_code == last_applied_contry_code_)
    return;
  last_applied_contry_code_ = country_code;
  CallJS("setCountryCode", country_code);
}

void ArcTermsOfServiceScreenHandler::TimezoneChanged(
    const icu::TimeZone& timezone) {
  UpdateTimeZone();
}

void ArcTermsOfServiceScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("arcTermsOfServiceScreenHeading", IDS_ARC_OOBE_TERMS_HEADING);
  builder->Add("arcTermsOfServiceScreenDescription",
      IDS_ARC_OOBE_TERMS_DESCRIPTION);
  builder->Add("arcTermsOfServiceLoading", IDS_ARC_OOBE_TERMS_LOADING);
  builder->Add("arcTermsOfServiceError", IDS_ARC_OOBE_TERMS_LOAD_ERROR);
  builder->Add("arcTermsOfServiceSkipButton", IDS_ARC_OOBE_TERMS_BUTTON_SKIP);
  builder->Add("arcTermsOfServiceRetryButton", IDS_ARC_OOBE_TERMS_BUTTON_RETRY);
  builder->Add("arcTermsOfServiceAcceptButton",
               IDS_ARC_OOBE_TERMS_BUTTON_ACCEPT);
  builder->Add("arcPolicyLink", IDS_ARC_OPT_IN_PRIVACY_POLICY_LINK);
  builder->Add("arcTextBackupRestore", IDS_ARC_OPT_IN_DIALOG_BACKUP_RESTORE);
  builder->Add("arcTextLocationService", IDS_ARC_OPT_IN_LOCATION_SETTING);
  builder->Add("arcLearnMoreStatistics", IDS_ARC_OPT_IN_LEARN_MORE_STATISTICS);
  builder->Add("arcLearnMoreLocationService",
      IDS_ARC_OPT_IN_LEARN_MORE_LOCATION_SERVICES);
  builder->Add("arcLearnMoreBackupAndRestore",
      IDS_ARC_OPT_IN_LEARN_MORE_BACKUP_AND_RESTORE);
  builder->Add("arcOverlayClose", IDS_ARC_OOBE_TERMS_POPUP_HELP_CLOSE_BUTTON);
}

void ArcTermsOfServiceScreenHandler::OnMetricsModeChanged(bool enabled,
                                                          bool managed) {
  const Profile* const profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(profile);
  CHECK(user);

  const AccountId owner =
      user_manager::UserManager::Get()->GetOwnerAccountId();

  // Owner may not be set in case of initial account setup. Note, in case of
  // enterprise enrolled devices owner is always empty and we need to account
  // managed flag.
  const bool owner_profile = !owner.is_valid() || user->GetAccountId() == owner;

  if (owner_profile && !managed) {
    CallJS("setMetricsMode", base::string16(), false);
  } else {
    int message_id = enabled ?
        IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED :
        IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_DISABLED;
    CallJS("setMetricsMode", l10n_util::GetStringUTF16(message_id), true);
  }
}

void ArcTermsOfServiceScreenHandler::OnBackupAndRestoreModeChanged(
    bool enabled, bool managed) {
  CallJS("setBackupAndRestoreMode", enabled, managed);
}

void ArcTermsOfServiceScreenHandler::OnLocationServicesModeChanged(
    bool enabled, bool managed) {
  CallJS("setLocationServicesMode", enabled, managed);
}

void ArcTermsOfServiceScreenHandler::AddObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ArcTermsOfServiceScreenHandler::RemoveObserver(
    ArcTermsOfServiceScreenViewObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

void ArcTermsOfServiceScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  DoShow();
}

void ArcTermsOfServiceScreenHandler::Hide() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  pref_handler_.reset();
}

void ArcTermsOfServiceScreenHandler::Initialize() {
  if (!show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void ArcTermsOfServiceScreenHandler::DoShow() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  CHECK(profile);

  // Enable ARC to match ArcSessionManager logic. ArcSessionManager expects that
  // ARC is enabled (prefs::kArcEnabled = true) on showing Terms of Service. If
  // user accepts ToS then prefs::kArcEnabled is left activated. If user skips
  // ToS then prefs::kArcEnabled is automatically reset in ArcSessionManager.
  profile->GetPrefs()->SetBoolean(prefs::kArcEnabled, true);

  system::TimezoneSettings::GetInstance()->AddObserver(this);

  ShowScreen(kScreenId);

  UpdateTimeZone();
  pref_handler_.reset(new arc::ArcOptInPreferenceHandler(
      this, profile->GetPrefs()));
  pref_handler_->Start();

}

void ArcTermsOfServiceScreenHandler::HandleSkip() {
  for (auto& observer : observer_list_)
    observer.OnSkip();
}

void ArcTermsOfServiceScreenHandler::HandleAccept(
    bool enable_backup_restore,
    bool enable_location_services) {
  pref_handler_->EnableBackupRestore(enable_backup_restore);
  pref_handler_->EnableLocationService(enable_location_services);
  for (auto& observer : observer_list_)
    observer.OnAccept();
}

}  // namespace chromeos
