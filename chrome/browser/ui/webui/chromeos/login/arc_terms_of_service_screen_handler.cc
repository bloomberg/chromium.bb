// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"

#include "base/i18n/timezone.h"
#include "chrome/browser/chromeos/arc/optin/arc_optin_preference_handler.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kJsScreenPath[] = "login.ArcTermsOfServiceScreen";

}  // namespace

namespace chromeos {

ArcTermsOfServiceScreenHandler::ArcTermsOfServiceScreenHandler()
    : BaseScreenHandler(kJsScreenPath) {
}

ArcTermsOfServiceScreenHandler::~ArcTermsOfServiceScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

void ArcTermsOfServiceScreenHandler::RegisterMessages() {
  AddCallback("arcTermsOfServiceSkip",
              &ArcTermsOfServiceScreenHandler::HandleSkip);
  AddCallback("arcTermsOfServiceAccept",
              &ArcTermsOfServiceScreenHandler::HandleAccept);
}

void ArcTermsOfServiceScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("arcTermsOfServiceScreenHeading", IDS_ARC_OOBE_TERMS_HEADING);
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
  builder->Add("arcCountryCode", base::CountryCodeForCurrentTimezone());
}

void ArcTermsOfServiceScreenHandler::OnMetricsModeChanged(bool enabled,
                                                          bool managed) {
  int message_id;
  const Profile* const profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  CHECK(profile);
  const bool owner_profile = chromeos::ProfileHelper::IsOwnerProfile(profile);
  if (managed || !owner_profile) {
    message_id = enabled ?
        IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_ENABLED :
        IDS_ARC_OOBE_TERMS_DIALOG_METRICS_MANAGED_DISABLED;
  } else {
    message_id = enabled ?
        IDS_ARC_OOBE_TERMS_DIALOG_METRICS_ENABLED :
        IDS_ARC_OOBE_TERMS_DIALOG_METRICS_DISABLED;
  }

  const bool checkbox_enabled = !enabled && !managed && owner_profile;
  CallJS("setMetricsMode", l10n_util::GetStringUTF16(message_id),
      checkbox_enabled, enabled);
}

void ArcTermsOfServiceScreenHandler::OnBackupAndRestoreModeChanged(
    bool enabled, bool managed) {
  CallJS("setBackupAndRestoreMode", enabled, managed);
}

void ArcTermsOfServiceScreenHandler::OnLocationServicesModeChanged(
    bool enabled, bool managed) {
  CallJS("setLocationServicesMode", enabled, managed);
}

void ArcTermsOfServiceScreenHandler::SetDelegate(Delegate* screen) {
  screen_ = screen;
}

void ArcTermsOfServiceScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }

  DoShow();
}

void ArcTermsOfServiceScreenHandler::Hide() {
  pref_handler_.reset();
}

void ArcTermsOfServiceScreenHandler::Initialize() {
  if (!show_on_init_)
    return;

  Show();
  show_on_init_ = false;
}

void ArcTermsOfServiceScreenHandler::DoShow() {
  Profile* profile = Profile::FromBrowserContext(
      web_ui()->GetWebContents()->GetBrowserContext());
  CHECK(profile);

  pref_handler_.reset(new arc::ArcOptInPreferenceHandler(
      this, profile->GetPrefs()));
  pref_handler_->Start();
  ShowScreen(OobeScreen::SCREEN_ARC_TERMS_OF_SERVICE);
}

void ArcTermsOfServiceScreenHandler::HandleSkip() {
  if (screen_)
    screen_->OnSkip();
}

void ArcTermsOfServiceScreenHandler::HandleAccept(
    bool enable_metrics,
    bool enable_backup_restore,
    bool enable_location_services) {
  if (screen_) {
    screen_->OnAccept(
        enable_metrics, enable_backup_restore, enable_location_services);
  }
}

}  // namespace chromeos
