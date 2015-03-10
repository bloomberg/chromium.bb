// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
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
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "third_party/re2/re2/re2.h"

#if defined(OS_ANDROID)
#include "chrome/browser/password_manager/generated_password_saved_infobar_delegate_android.h"
#endif

using password_manager::ContentPasswordManagerDriverFactory;
using password_manager::PasswordManagerInternalsService;
using password_manager::PasswordManagerInternalsServiceFactory;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromePasswordManagerClient);

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

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
      password_manager_(this),
      driver_factory_(nullptr),
      credential_manager_dispatcher_(web_contents, this),
      observer_(nullptr),
      can_use_log_router_(false),
      autofill_sync_state_(ALLOW_SYNC_CREDENTIALS),
      sync_credential_was_filtered_(false) {
  ContentPasswordManagerDriverFactory::CreateForWebContents(web_contents, this,
                                                            autofill_client);
  driver_factory_ =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);

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
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             password_manager::switches::kEnableAutomaticPasswordSaving) &&
         chrome::VersionInfo::GetChannel() ==
             chrome::VersionInfo::CHANNEL_UNKNOWN;
}

bool ChromePasswordManagerClient::IsPasswordManagerEnabledForCurrentPage()
    const {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry) {
    // TODO(gcasto): Determine if fix for crbug.com/388246 is relevant here.
    return true;
  }

  // Disable the password manager for online password management.
  if (IsURLPasswordWebsiteReauth(entry->GetURL()))
    return false;

  if (EnabledForSyncSignin())
    return true;

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

std::string ChromePasswordManagerClient::GetSyncUsername() const {
  return password_manager_sync_metrics::GetSyncUsername(profile_);
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

bool ChromePasswordManagerClient::PromptUserToSavePassword(
    scoped_ptr<password_manager::PasswordFormManager> form_to_save,
    password_manager::CredentialSourceType type) {
  // Save password infobar and the password bubble prompts in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!BrowsingDataHelper::IsWebScheme(
      web_contents()->GetLastCommittedURL().scheme())) {
    return false;
  }

  if (IsTheHotNewBubbleUIEnabled()) {
    ManagePasswordsUIController* manage_passwords_ui_controller =
        ManagePasswordsUIController::FromWebContents(web_contents());
    manage_passwords_ui_controller->OnPasswordSubmitted(form_to_save.Pass());
  } else {
    // TODO(melandory): If type is CREDENTIAL_SOURCE_API then new bubble should
    // be shown.
    std::string uma_histogram_suffix(
        password_manager::metrics_util::GroupIdToString(
            password_manager::metrics_util::MonitoredDomainGroupId(
                form_to_save->realm(), GetPrefs())));
    SavePasswordInfoBarDelegate::Create(
        web_contents(), form_to_save.Pass(), uma_histogram_suffix, type);
  }
  return true;
}

bool ChromePasswordManagerClient::PromptUserToChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_forms,
    ScopedVector<autofill::PasswordForm> federated_forms,
    const GURL& origin,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
  return ManagePasswordsUIController::FromWebContents(web_contents())->
      OnChooseCredentials(local_forms.Pass(), federated_forms.Pass(), origin,
                          callback);
}

void ChromePasswordManagerClient::NotifyUserAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
  DCHECK(!local_forms.empty());
  ManagePasswordsUIController::FromWebContents(web_contents())->
      OnAutoSignin(local_forms.Pass());
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<password_manager::PasswordFormManager> saved_form) {
#if defined(OS_ANDROID)
  GeneratedPasswordSavedInfoBarDelegateAndroid::Create(web_contents());
#else
  if (IsTheHotNewBubbleUIEnabled()) {
    ManagePasswordsUIController* manage_passwords_ui_controller =
        ManagePasswordsUIController::FromWebContents(web_contents());
    manage_passwords_ui_controller->OnAutomaticPasswordSave(
        saved_form.Pass());
  }
#endif
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

void ChromePasswordManagerClient::HidePasswordGenerationPopup() {
  if (popup_controller_)
    popup_controller_->HideAndDestroy();
}

PrefService* ChromePasswordManagerClient::GetPrefs() {
  return profile_->GetPrefs();
}

password_manager::PasswordStore*
ChromePasswordManagerClient::GetPasswordStore() const {
  // Always use EXPLICIT_ACCESS as the password manager checks IsOffTheRecord
  // itself when it shouldn't access the PasswordStore.
  // TODO(gcasto): Is is safe to change this to
  // ServiceAccessType::IMPLICIT_ACCESS?
  return PasswordStoreFactory::GetForProfile(
             profile_, ServiceAccessType::EXPLICIT_ACCESS).get();
}

base::FieldTrial::Probability
ChromePasswordManagerClient::GetProbabilityForExperiment(
    const std::string& experiment_name) const {
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

bool ChromePasswordManagerClient::IsPasswordSyncEnabled(
    password_manager::CustomPassphraseState state) const {
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  if (sync_service && sync_service->HasSyncSetupCompleted() &&
      sync_service->SyncActive() &&
      sync_service->GetActiveDataTypes().Has(syncer::PASSWORDS)) {
    if (sync_service->IsUsingSecondaryPassphrase()) {
      return state == password_manager::ONLY_CUSTOM_PASSPHRASE;
    } else {
      return state == password_manager::WITHOUT_CUSTOM_PASSPHRASE;
    }
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
    const std::string& text) const {
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

bool ChromePasswordManagerClient::WasLastNavigationHTTPError() const {
  DCHECK(web_contents());

  scoped_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  if (IsLoggingActive()) {
    logger.reset(new password_manager::BrowserSavePasswordProgressLogger(this));
    logger->LogMessage(
        Logger::STRING_WAS_LAST_NAVIGATION_HTTP_ERROR_METHOD);
  }

  content::NavigationEntry* entry =
      web_contents()->GetController().GetVisibleEntry();
  if (!entry)
    return false;
  int http_status_code = entry->GetHttpStatusCode();

  if (logger)
    logger->LogNumber(Logger::STRING_HTTP_STATUS_CODE, http_status_code);

  if (http_status_code >= 400 && http_status_code < 600)
    return true;
  return false;
}

bool ChromePasswordManagerClient::DidLastPageLoadEncounterSSLErrors() const {
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return false;

  return net::IsCertStatusError(entry->GetSSL().cert_status);
}

bool ChromePasswordManagerClient::IsOffTheRecord() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

password_manager::PasswordManager*
ChromePasswordManagerClient::GetPasswordManager() {
  return &password_manager_;
}

autofill::AutofillManager*
ChromePasswordManagerClient::GetAutofillManagerForMainFrame() {
  autofill::ContentAutofillDriverFactory* factory =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents());
  return factory
             ? factory->DriverForFrame(web_contents()->GetMainFrame())
                   ->autofill_manager()
             : nullptr;
}

void ChromePasswordManagerClient::SetTestObserver(
    autofill::PasswordGenerationPopupObserver* observer) {
  observer_ = observer;
}

bool ChromePasswordManagerClient::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(ChromePasswordManagerClient, message,
                                   render_frame_host)
    // Autofill messages:
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordGenerationPopup,
                        ShowPasswordGenerationPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_ShowPasswordEditingPopup,
                        ShowPasswordEditingPopup)
    IPC_END_MESSAGE_MAP()

    IPC_BEGIN_MESSAGE_MAP(ChromePasswordManagerClient, message)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_HidePasswordGenerationPopup,
                        HidePasswordGenerationPopup)
    IPC_MESSAGE_HANDLER(AutofillHostMsg_PasswordAutofillAgentConstructed,
                        NotifyRendererOfLoggingAvailability)
    // Default:
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
    content::RenderFrameHost* render_frame_host,
    const gfx::RectF& bounds,
    int max_length,
    const autofill::PasswordForm& form) {
  // TODO(gcasto): Validate data in PasswordForm.

  gfx::RectF element_bounds_in_screen_space = GetBoundsInScreenSpace(bounds);

  popup_controller_ =
      autofill::PasswordGenerationPopupControllerImpl::GetOrCreate(
          popup_controller_, element_bounds_in_screen_space, form, max_length,
          &password_manager_,
          driver_factory_->GetDriverForFrame(render_frame_host), observer_,
          web_contents(), web_contents()->GetNativeView());
  popup_controller_->Show(true /* display_password */);
}

void ChromePasswordManagerClient::ShowPasswordEditingPopup(
    content::RenderFrameHost* render_frame_host,
    const gfx::RectF& bounds,
    const autofill::PasswordForm& form) {
  gfx::RectF element_bounds_in_screen_space = GetBoundsInScreenSpace(bounds);
  popup_controller_ =
      autofill::PasswordGenerationPopupControllerImpl::GetOrCreate(
          popup_controller_, element_bounds_in_screen_space, form,
          0,  // Unspecified max length.
          &password_manager_,
          driver_factory_->GetDriverForFrame(render_frame_host), observer_,
          web_contents(), web_contents()->GetNativeView());
  popup_controller_->Show(false /* display_password */);
}

void ChromePasswordManagerClient::NotifyRendererOfLoggingAvailability() {
  if (!web_contents())
    return;

  web_contents()->GetRenderViewHost()->Send(new AutofillMsg_SetLoggingState(
      web_contents()->GetRenderViewHost()->GetRoutingID(),
      can_use_log_router_));
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

bool ChromePasswordManagerClient::IsURLPasswordWebsiteReauth(
    const GURL& url) const {
  if (url.GetOrigin() != GaiaUrls::GetInstance()->gaia_url().GetOrigin())
    return false;

  // "rart" param signals this page is for transactional reauth.
  std::string param_value;
  if (!net::GetValueForKeyInQuery(url, "rart", &param_value))
    return false;

  // Check the "continue" param to see if this reauth page is for the passwords
  // website.
  param_value.clear();
  if (!net::GetValueForKeyInQuery(url, "continue", &param_value))
    return false;

  // All password sites, including test sites, have autofilling disabled.
  CR_DEFINE_STATIC_LOCAL(RE2, account_dashboard_pattern,
                         ("passwords(-([a-z-]+\\.corp))?\\.google\\.com"));

  return RE2::FullMatch(GURL(param_value).host(), account_dashboard_pattern);
}

bool ChromePasswordManagerClient::IsTheHotNewBubbleUIEnabled() {
#if !defined(USE_AURA) && !defined(OS_MACOSX)
  return false;
#endif
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
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
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
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

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
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

const GURL& ChromePasswordManagerClient::GetMainFrameURL() const {
  return web_contents()->GetVisibleURL();
}
