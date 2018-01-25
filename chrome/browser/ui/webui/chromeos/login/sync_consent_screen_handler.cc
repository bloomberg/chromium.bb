// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"

#include "chrome/browser/chromeos/login/screens/sync_consent_screen.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"

namespace {

const char kJsScreenPath[] = "login.SyncConsentScreen";

}  // namespace

namespace chromeos {

SyncConsentScreenHandler::SyncConsentScreenHandler()
    : BaseScreenHandler(kScreenId) {
  set_call_js_prefix(kJsScreenPath);
}

SyncConsentScreenHandler::~SyncConsentScreenHandler() {}

void SyncConsentScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("syncConsentScreenTitle", IDS_LOGIN_SYNC_CONSENT_SCREEN_TITLE);
  builder->Add("syncConsentScreenChromeSyncName",
               IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_NAME);
  builder->Add("syncConsentScreenChromeSyncDescription",
               IDS_LOGIN_SYNC_CONSENT_SCREEN_CHROME_SYNC_DESCRIPTION);
  builder->Add("syncConsentScreenPersonalizeGoogleServicesName",
               IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_NAME);
  builder->Add(
      "syncConsentScreenPersonalizeGoogleServicesDescription",
      IDS_LOGIN_SYNC_CONSENT_SCREEN_PERSONALIZE_GOOGLE_SERVICES_DESCRIPTION);
  builder->Add("syncConsentScreenSettingsLink",
               IDS_LOGIN_SYNC_CONSENT_SCREEN_SETTINGS_LINK);
  builder->Add("syncConsentSettingsDialogTitle",
               IDS_LOGIN_SYNC_CONSENT_SETTINGS_TITLE);
  builder->Add("syncConsentSettingsDialogSubTitle",
               IDS_LOGIN_SYNC_CONSENT_SETTINGS_SUBTITLE);
  builder->Add("syncConsentSyncAllOptionTitle",
               IDS_LOGIN_SYNC_CONSENT_SYNC_ALL_OPTION);
  builder->Add("syncConsentSyncAllOptionOn",
               IDS_LOGIN_SYNC_CONSENT_SYNC_ALL_OPTION_ON);
  builder->Add("syncConsentSyncAllOptionOff",
               IDS_LOGIN_SYNC_CONSENT_SYNC_ALL_OPTION_OFF);
  builder->Add("syncConsentSettingsStatusSyncAllOn",
               IDS_LOGIN_SYNC_CONSENT_STATUS_SYNC_ALL_ON);
  builder->Add("syncConsentSettingsStatusSyncAllOff",
               IDS_LOGIN_SYNC_CONSENT_STATUS_SYNC_ALL_OFF);
  builder->Add("syncConsentSettingsSaveAndContinue",
               IDS_LOGIN_SYNC_CONSENT_SAVE_AND_CONTINUE);
}

void SyncConsentScreenHandler::RegisterMessages() {
  BaseScreenHandler::RegisterMessages();

  AddCallback("syncEverythingChanged",
              &SyncConsentScreenHandler::HandleSyncEverythingChanged);
}

void SyncConsentScreenHandler::Bind(SyncConsentScreen* screen) {
  screen_ = screen;
  BaseScreenHandler::SetBaseScreen(screen);
}

void SyncConsentScreenHandler::Show() {
  ShowScreen(kScreenId);
}

void SyncConsentScreenHandler::Hide() {}

void SyncConsentScreenHandler::Initialize() {}

void SyncConsentScreenHandler::HandleSyncEverythingChanged(
    bool sync_everything) {
  screen_->SetSyncAllValue(sync_everything);
}

void SyncConsentScreenHandler::OnUserPrefKnown(bool sync_everything,
                                               bool is_managed) {
  CallJS("onUserSyncPrefsKnown", sync_everything, is_managed);
}

}  // namespace chromeos
