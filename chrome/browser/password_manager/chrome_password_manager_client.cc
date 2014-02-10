// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/password_manager/password_form_manager.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_util.h"
#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents.h"

namespace {

void ReportOsPassword() {
  password_manager_util::OsPasswordStatus status =
      password_manager_util::GetOsPasswordStatus();

  UMA_HISTOGRAM_ENUMERATION("PasswordManager.OsPasswordStatus",
                            status,
                            password_manager_util::MAX_PASSWORD_STATUS);
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromePasswordManagerClient);

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents)
    : web_contents_(web_contents), driver_(web_contents, this) {
  // Avoid checking OS password until later on in browser startup
  // since it calls a few Windows APIs.
  base::MessageLoopProxy::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ReportOsPassword),
      base::TimeDelta::FromSeconds(10));
}

ChromePasswordManagerClient::~ChromePasswordManagerClient() {}

void ChromePasswordManagerClient::PromptUserToSavePassword(
    PasswordFormManager* form_to_save) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSavePasswordBubble)) {
    ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
        ManagePasswordsBubbleUIController::FromWebContents(web_contents_);
    if (manage_passwords_bubble_ui_controller) {
      manage_passwords_bubble_ui_controller->OnPasswordSubmitted(form_to_save);
    } else {
      delete form_to_save;
    }
  } else {
    std::string uma_histogram_suffix(
        password_manager_metrics_util::GroupIdToString(
            password_manager_metrics_util::MonitoredDomainGroupId(
                form_to_save->realm(), GetProfile()->GetPrefs())));
    SavePasswordInfoBarDelegate::Create(
        web_contents_, form_to_save, uma_histogram_suffix);
  }
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    const autofill::PasswordFormMap& best_matches) const {
  ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
      ManagePasswordsBubbleUIController::FromWebContents(web_contents_);
  if (manage_passwords_bubble_ui_controller &&
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableSavePasswordBubble)) {
    manage_passwords_bubble_ui_controller->OnPasswordAutofilled(best_matches);
  }
}

Profile* ChromePasswordManagerClient::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

PrefService* ChromePasswordManagerClient::GetPrefs() {
  return GetProfile()->GetPrefs();
}

PasswordManagerDriver* ChromePasswordManagerClient::GetDriver() {
  return &driver_;
}

base::FieldTrial::Probability
ChromePasswordManagerClient::GetProbabilityForExperiment(
    const std::string& experiment_name) {
  base::FieldTrial::Probability enabled_probability = 0;
  if (experiment_name == PasswordManager::kOtherPossibleUsernamesExperiment) {
    switch (chrome::VersionInfo::GetChannel()) {
      case chrome::VersionInfo::CHANNEL_DEV:
      case chrome::VersionInfo::CHANNEL_BETA:
        enabled_probability = 50;
        break;
      default:
        break;
    }
  }
  return enabled_probability;
}

bool ChromePasswordManagerClient::IsPasswordSyncEnabled() {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(GetProfile());
  if (sync_service && sync_service->HasSyncSetupCompleted())
    return sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS);
  return false;
}

// static
PasswordGenerationManager*
ChromePasswordManagerClient::GetGenerationManagerFromWebContents(
    content::WebContents* contents) {
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(contents);
  if (!client)
    return NULL;
  return client->GetDriver()->GetPasswordGenerationManager();
}

// static
PasswordManager* ChromePasswordManagerClient::GetManagerFromWebContents(
    content::WebContents* contents) {
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(contents);
  if (!client)
    return NULL;
  return client->GetDriver()->GetPasswordManager();
}
