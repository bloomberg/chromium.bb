// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_manager/password_manager_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/password_manager/sync_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/password_authentication_manager.h"
#endif  // OS_ANDROID

using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerInternalsServiceFactory;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromePasswordManagerClient);

// static
void ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
    content::WebContents* contents,
    autofill::AutofillClient* autofill_client) {
  if (FromWebContents(contents))
    return;

  contents->SetUserData(
      UserDataKey(),
      new ChromePasswordManagerClient(contents, autofill_client));
}

ChromePasswordManagerClient::ChromePasswordManagerClient(
    content::WebContents* web_contents,
    autofill::AutofillClient* autofill_client)
    : content::WebContentsObserver(web_contents),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      driver_(web_contents, this, autofill_client),
      observer_(NULL),
      weak_factory_(this),
      can_use_log_router_(false),
      autofill_sync_state_(ALLOW_SYNC_CREDENTIALS),
      sync_credential_was_filtered_(false) {
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile_);
  if (service)
    can_use_log_router_ = service->RegisterClient(this);
  SetUpAutofillSyncState();
}

ChromePasswordManagerClient::~ChromePasswordManagerClient() {
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile_);
  if (service)
    service->UnregisterClient(this);
}

bool ChromePasswordManagerClient::IsAutomaticPasswordSavingEnabled() const {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      password_manager::switches::kEnableAutomaticPasswordSaving) &&
         chrome::VersionInfo::GetChannel() ==
             chrome::VersionInfo::CHANNEL_UNKNOWN;
}

bool ChromePasswordManagerClient::IsPasswordManagerEnabledForCurrentPage()
    const {
  if (EnabledForSyncSignin())
    return true;

  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    // TODO(gcasto): Determine if fix for crbug.com/388246 is relevant here.
    return true;
  }
  // Do not fill nor save password when a user is signing in for sync. This
  // is because users need to remember their password if they are syncing as
  // this is effectively their master password.
  return entry->GetURL().host() != chrome::kChromeUIChromeSigninHost;
}

bool ChromePasswordManagerClient::ShouldFilterAutofillResult(
    const autofill::PasswordForm& form) {
  if (!IsSyncAccountCredential(base::UTF16ToUTF8(form.username_value),
                               form.signon_realm))
    return false;

  if (autofill_sync_state_ == DISALLOW_SYNC_CREDENTIALS) {
    sync_credential_was_filtered_ = true;
    return true;
  }

  if (autofill_sync_state_ == DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH &&
      LastLoadWasTransactionalReauthPage()) {
    sync_credential_was_filtered_ = true;
    return true;
  }

  return false;
}

bool ChromePasswordManagerClient::IsSyncAccountCredential(
    const std::string& username, const std::string& origin) const {
  return password_manager_sync_metrics::IsSyncAccountCredential(
      profile_, username, origin);
}

void ChromePasswordManagerClient::AutofillResultsComputed() {
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.SyncCredentialFiltered",
                        sync_credential_was_filtered_);
  sync_credential_was_filtered_ = false;
}

void ChromePasswordManagerClient::PromptUserToSavePassword(
    scoped_ptr<password_manager::PasswordFormManager> form_to_save) {
  if (IsTheHotNewBubbleUIEnabled()) {
    ManagePasswordsUIController* manage_passwords_ui_controller =
        ManagePasswordsUIController::FromWebContents(web_contents());
    manage_passwords_ui_controller->OnPasswordSubmitted(form_to_save.Pass());
  } else {
    std::string uma_histogram_suffix(
        password_manager::metrics_util::GroupIdToString(
            password_manager::metrics_util::MonitoredDomainGroupId(
                form_to_save->realm(), GetPrefs())));
    SavePasswordInfoBarDelegate::Create(
        web_contents(), form_to_save.Pass(), uma_histogram_suffix);
  }
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<password_manager::PasswordFormManager> saved_form) {
  if (IsTheHotNewBubbleUIEnabled()) {
    ManagePasswordsUIController* manage_passwords_ui_controller =
        ManagePasswordsUIController::FromWebContents(web_contents());
    manage_passwords_ui_controller->OnAutomaticPasswordSave(
        saved_form.Pass());
  }
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    const autofill::PasswordFormMap& best_matches) const {
  ManagePasswordsUIController* manage_passwords_ui_controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  if (manage_passwords_ui_controller && IsTheHotNewBubbleUIEnabled())
    manage_passwords_ui_controller->OnPasswordAutofilled(best_matches);
}

void ChromePasswordManagerClient::PasswordAutofillWasBlocked(
    const autofill::PasswordFormMap& best_matches) const {
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_contents());
  if (controller && IsTheHotNewBubbleUIEnabled())
    controller->OnBlacklistBlockedAutofill(best_matches);
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

void ChromePasswordManagerClient::HidePasswordGenerationPopup() {
  if (popup_controller_)
    popup_controller_->HideAndDestroy();
}

PrefService* ChromePasswordManagerClient::GetPrefs() {
  return profile_->GetPrefs();
}

password_manager::PasswordStore*
ChromePasswordManagerClient::GetPasswordStore() {
  // Always use EXPLICIT_ACCESS as the password manager checks IsOffTheRecord
  // itself when it shouldn't access the PasswordStore.
  // TODO(gcasto): Is is safe to change this to Profile::IMPLICIT_ACCESS?
  return PasswordStoreFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS)
      .get();
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
      ProfileSyncServiceFactory::GetForProfile(profile_);
  // Don't consider sync enabled if the user has a custom passphrase. See
  // crbug.com/358998 for more details.
  if (sync_service &&
      sync_service->HasSyncSetupCompleted() &&
      sync_service->sync_initialized() &&
      !sync_service->IsUsingSecondaryPassphrase()) {
    return sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS);
  }
  return false;
}

void ChromePasswordManagerClient::OnLogRouterAvailabilityChanged(
    bool router_can_be_used) {
  if (can_use_log_router_ == router_can_be_used)
    return;
  can_use_log_router_ = router_can_be_used;

  NotifyRendererOfLoggingAvailability();
}

void ChromePasswordManagerClient::LogSavePasswordProgress(
    const std::string& text) {
  if (!IsLoggingActive())
    return;
  PasswordManagerInternalsService* service =
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile_);
  if (service)
    service->ProcessLog(text);
}

bool ChromePasswordManagerClient::IsLoggingActive() const {
  // WebUI tabs do not need to log password saving progress. In particular, the
  // internals page itself should not send any logs.
  return can_use_log_router_ && !web_contents()->GetWebUI();
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
    IPC_MESSAGE_HANDLER(AutofillHostMsg_PasswordAutofillAgentConstructed,
                        NotifyRendererOfLoggingAvailability)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

gfx::RectF ChromePasswordManagerClient::GetBoundsInScreenSpace(
    const gfx::RectF& bounds) {
  gfx::Rect client_area = web_contents()->GetContainerBounds();
  return bounds + client_area.OffsetFromOrigin();
}

void ChromePasswordManagerClient::ShowPasswordGenerationPopup(
    const gfx::RectF& bounds,
    int max_length,
    const autofill::PasswordForm& form) {
  // TODO(gcasto): Validate data in PasswordForm.

  // Not yet implemented on other platforms.
#if defined(USE_AURA) || defined(OS_MACOSX)
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
          web_contents()->GetNativeView());
  popup_controller_->Show(true /* display_password */);
#endif  // defined(USE_AURA) || defined(OS_MACOSX)
}

void ChromePasswordManagerClient::ShowPasswordEditingPopup(
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form) {
  gfx::RectF element_bounds_in_screen_space = GetBoundsInScreenSpace(bounds);
  // Not yet implemented on other platforms.
#if defined(USE_AURA) || defined(OS_MACOSX)
  popup_controller_ =
      autofill::PasswordGenerationPopupControllerImpl::GetOrCreate(
          popup_controller_,
          element_bounds_in_screen_space,
          form,
          0,  // Unspecified max length.
          driver_.GetPasswordManager(),
          observer_,
          web_contents(),
          web_contents()->GetNativeView());
  popup_controller_->Show(false /* display_password */);
#endif  // defined(USE_AURA) || defined(OS_MACOSX)
}

void ChromePasswordManagerClient::NotifyRendererOfLoggingAvailability() {
  if (!web_contents())
    return;

  web_contents()->GetRenderViewHost()->Send(new AutofillMsg_SetLoggingState(
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      can_use_log_router_));
}

void ChromePasswordManagerClient::CommitFillPasswordForm(
    autofill::PasswordFormFillData* data) {
  driver_.FillPasswordForm(*data);
}

bool ChromePasswordManagerClient::LastLoadWasTransactionalReauthPage() const {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;

  if (entry->GetURL().GetOrigin() !=
      GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  // "rart" is the transactional reauth paramter.
  std::string ignored_value;
  return net::GetValueForKeyInQuery(entry->GetURL(),
                                    "rart",
                                    &ignored_value);
}

bool ChromePasswordManagerClient::IsTheHotNewBubbleUIEnabled() {
#if !defined(USE_AURA)
  return false;
#endif
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableSavePasswordBubble))
    return false;

  if (command_line->HasSwitch(switches::kEnableSavePasswordBubble))
    return true;

  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordManagerUI");

  // The bubble should be the default case that runs on the bots.
  return group_name != "Infobar";
}

bool ChromePasswordManagerClient::EnabledForSyncSignin() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          password_manager::switches::kDisableManagerForSyncSignin))
    return false;

  if (command_line->HasSwitch(
          password_manager::switches::kEnableManagerForSyncSignin))
    return true;

  // Default is enabled.
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordManagerStateForSyncSignin");
  return group_name != "Disabled";
}

void ChromePasswordManagerClient::SetUpAutofillSyncState() {
  std::string group_name =
      base::FieldTrialList::FindFullName("AutofillSyncCredential");

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(
          password_manager::switches::kAllowAutofillSyncCredential)) {
    autofill_sync_state_ = ALLOW_SYNC_CREDENTIALS;
    return;
  }
  if (command_line->HasSwitch(
          password_manager::switches::
          kDisallowAutofillSyncCredentialForReauth)) {
    autofill_sync_state_ = DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH;
    return;
  }
  if (command_line->HasSwitch(
          password_manager::switches::kDisallowAutofillSyncCredential)) {
    autofill_sync_state_ = DISALLOW_SYNC_CREDENTIALS;
    return;
  }

  if (group_name == "DisallowSyncCredentialsForReauth") {
    autofill_sync_state_ = DISALLOW_SYNC_CREDENTIALS_FOR_REAUTH;
  } else if (group_name == "DisallowSyncCredentials") {
      autofill_sync_state_ = DISALLOW_SYNC_CREDENTIALS;
  } else {
    // Allow by default.
    autofill_sync_state_ = ALLOW_SYNC_CREDENTIALS;
  }
}
