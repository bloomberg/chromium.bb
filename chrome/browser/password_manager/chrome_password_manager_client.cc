// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/password_manager/password_manager_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_logger.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ipc/ipc_message_macros.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/password_authentication_manager.h"
#endif  // OS_ANDROID

namespace {

bool IsTheHotNewBubbleUIEnabled() {
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordManagerUI");

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableSavePasswordBubble))
    return false;

  if (command_line->HasSwitch(switches::kEnableSavePasswordBubble))
    return true;

  return group_name == "Bubble";
}

} // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromePasswordManagerClient);

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      driver_(web_contents, this),
      observer_(NULL),
      weak_factory_(this),
      logger_(NULL) {}

ChromePasswordManagerClient::~ChromePasswordManagerClient() {}

void ChromePasswordManagerClient::PromptUserToSavePassword(
    password_manager::PasswordFormManager* form_to_save) {
  if (IsTheHotNewBubbleUIEnabled()) {
    ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
        ManagePasswordsBubbleUIController::FromWebContents(web_contents());
    if (manage_passwords_bubble_ui_controller) {
      manage_passwords_bubble_ui_controller->OnPasswordSubmitted(form_to_save);
    } else {
      delete form_to_save;
    }
  } else {
    std::string uma_histogram_suffix(
        password_manager::metrics_util::GroupIdToString(
            password_manager::metrics_util::MonitoredDomainGroupId(
                form_to_save->realm(), GetPrefs())));
    SavePasswordInfoBarDelegate::Create(
        web_contents(), form_to_save, uma_histogram_suffix);
  }
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    const autofill::PasswordFormMap& best_matches) const {
  ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
      ManagePasswordsBubbleUIController::FromWebContents(web_contents());
  if (manage_passwords_bubble_ui_controller && IsTheHotNewBubbleUIEnabled())
    manage_passwords_bubble_ui_controller->OnPasswordAutofilled(best_matches);
}

void ChromePasswordManagerClient::PasswordAutofillWasBlocked() const {
  ManagePasswordsBubbleUIController* manage_passwords_bubble_ui_controller =
      ManagePasswordsBubbleUIController::FromWebContents(web_contents());
  if (manage_passwords_bubble_ui_controller && IsTheHotNewBubbleUIEnabled())
    manage_passwords_bubble_ui_controller->OnBlacklistBlockedAutofill();
}

void ChromePasswordManagerClient::AuthenticateAutofillAndFillForm(
      scoped_ptr<autofill::PasswordFormFillData> fill_data) {
#if defined(OS_ANDROID)
  PasswordAuthenticationManager::AuthenticatePasswordAutofill(
      web_contents(),
      base::Bind(&ChromePasswordManagerClient::CommitFillPasswordForm,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(fill_data.release())));
#else
  // Additional authentication is currently only available for Android, so all
  // other plaftorms should just fill the password form directly.
  CommitFillPasswordForm(fill_data.get());
#endif  // OS_ANDROID
}

Profile* ChromePasswordManagerClient::GetProfile() {
  return Profile::FromBrowserContext(web_contents()->GetBrowserContext());
}

void ChromePasswordManagerClient::HidePasswordGenerationPopup() {
  if (popup_controller_)
    popup_controller_->HideAndDestroy();
}

PrefService* ChromePasswordManagerClient::GetPrefs() {
  return GetProfile()->GetPrefs();
}

password_manager::PasswordStore*
ChromePasswordManagerClient::GetPasswordStore() {
  // Always use EXPLICIT_ACCESS as the password manager checks IsOffTheRecord
  // itself when it shouldn't access the PasswordStore.
  // TODO(gcasto): Is is safe to change this to Profile::IMPLICIT_ACCESS?
  return PasswordStoreFactory::GetForProfile(GetProfile(),
                                             Profile::EXPLICIT_ACCESS).get();
}

password_manager::PasswordManagerDriver*
ChromePasswordManagerClient::GetDriver() {
  return &driver_;
}

base::FieldTrial::Probability
ChromePasswordManagerClient::GetProbabilityForExperiment(
    const std::string& experiment_name) {
  base::FieldTrial::Probability enabled_probability = 0;
  if (experiment_name ==
      password_manager::PasswordManager::kOtherPossibleUsernamesExperiment) {
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

void ChromePasswordManagerClient::SetLogger(
    password_manager::PasswordManagerLogger* logger) {
  // We should never be replacing one logger with a different one, because that
  // will leave the first without further updates, and the user likely confused.
  // TODO(vabr): For the reason above, before moving the internals page from
  // behind the flag, make sure to restrict the number of internals page
  // instances to 1 in normal profiles, and 0 in incognito.
  DCHECK(!logger || !logger_);
  logger_ = logger;
}

void ChromePasswordManagerClient::LogSavePasswordProgress(
    const std::string& text) {
  if (logger_)
    logger_->LogSavePasswordProgress(text);
}

// static
password_manager::PasswordGenerationManager*
ChromePasswordManagerClient::GetGenerationManagerFromWebContents(
    content::WebContents* contents) {
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(contents);
  if (!client)
    return NULL;
  return client->GetDriver()->GetPasswordGenerationManager();
}

// static
password_manager::PasswordManager*
ChromePasswordManagerClient::GetManagerFromWebContents(
    content::WebContents* contents) {
  ChromePasswordManagerClient* client =
      ChromePasswordManagerClient::FromWebContents(contents);
  if (!client)
    return NULL;
  return client->GetDriver()->GetPasswordManager();
}

void ChromePasswordManagerClient::SetTestObserver(
    autofill::PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

bool ChromePasswordManagerClient::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ChromePasswordManagerClient, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordGenerationPopup,
                        ShowPasswordGenerationPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordEditingPopup,
                        ShowPasswordEditingPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_HidePasswordGenerationPopup,
                        HidePasswordGenerationPopup)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

gfx::RectF ChromePasswordManagerClient::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area;
  web_contents()->GetView()->GetContainerBounds(&client_area);
  return bounds + client_area.OffsetFromOrigin();
}

void ChromePasswordManagerClient::ShowPasswordGenerationPopup(
    const gfx::RectF& bounds,
    int max_length,
    const autofill::PasswordForm& form) {
  // TODO(gcasto): Validate data in PasswordForm.

  // Only implemented for Aura right now.
#if defined(USE_AURA)
  gfx::RectF element_bounds_in_screen_space = GetBoundsInScreenSpace(bounds);

  popup_controller_ =
      autofill::PasswordGenerationPopupControllerImpl::GetOrCreate(
          popup_controller_,
          element_bounds_in_screen_space,
          form,
          max_length,
          driver_.GetPasswordManager(),
          observer_,
          web_contents(),
          web_contents()->GetView()->GetNativeView());
  popup_controller_->Show(true /* display_password */);
#endif  // #if defined(USE_AURA)
}

void ChromePasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form) {
  // Only implemented for Aura right now.
#if defined(USE_AURA)
  gfx::RectF element_bounds_in_screen_space = GetBoundsInScreenSpace(bounds);

  popup_controller_ =
      autofill::PasswordGenerationPopupControllerImpl::GetOrCreate(
          popup_controller_,
          element_bounds_in_screen_space,
          form,
          0,  // Unspecified max length.
          driver_.GetPasswordManager(),
          observer_,
          web_contents(),
          web_contents()->GetView()->GetNativeView());
  popup_controller_->Show(false /* display_password */);
#endif  // #if defined(USE_AURA)
}

void ChromePasswordManagerClient::CommitFillPasswordForm(
    autofill::PasswordFormFillData* data) {
  driver_.FillPasswordForm(*data);
}
