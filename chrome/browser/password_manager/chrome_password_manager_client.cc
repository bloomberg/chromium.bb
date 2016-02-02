// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/password_generation_popup_controller_impl.h"
#include "chrome/browser/ui/passwords/passwords_client_ui_delegate.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/content/common/credential_manager_messages.h"
#include "components/password_manager/core/browser/browser_save_password_progress_logger.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_settings_migration_experiment.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_switches.h"
#include "components/password_manager/sync/browser/password_sync_util.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/url_util.h"
#include "third_party/re2/src/re2/re2.h"

#if defined(OS_MACOSX) || BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/password_manager/account_chooser_dialog_android.h"
#include "chrome/browser/password_manager/generated_password_saved_infobar_delegate_android.h"
#include "chrome/browser/ui/android/snackbars/auto_signin_prompt_controller.h"
#endif

using password_manager::ContentPasswordManagerDriverFactory;
using password_manager::PasswordManagerInternalsService;

// Shorten the name to spare line breaks. The code provides enough context
// already.
typedef autofill::SavePasswordProgressLogger Logger;

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ChromePasswordManagerClient);

namespace {

const sync_driver::SyncService* GetSyncService(Profile* profile) {
  if (ProfileSyncServiceFactory::HasProfileSyncService(profile))
    return ProfileSyncServiceFactory::GetForProfile(profile);
  return nullptr;
}

const SigninManagerBase* GetSigninManager(Profile* profile) {
  return SigninManagerFactory::GetForProfile(profile);
}

// This routine is called when PasswordManagerClient is constructed.
// Currently we report metrics only once at startup. We require
// that this is only ever called from a single thread in order to
// avoid needing to lock (a static boolean flag is then sufficient to
// guarantee running only once).
void ReportMetrics(bool password_manager_enabled,
                   password_manager::PasswordManagerClient* client,
                   Profile* profile) {
  static base::PlatformThreadId initial_thread_id =
      base::PlatformThread::CurrentId();
  DCHECK_EQ(base::PlatformThread::CurrentId(), initial_thread_id);

  static bool ran_once = false;
  if (ran_once)
    return;
  ran_once = true;

  password_manager::PasswordStore* store = client->GetPasswordStore();
  // May be null in tests.
  if (store) {
    store->ReportMetrics(
        password_manager::sync_util::GetSyncUsernameIfSyncingPasswords(
            GetSyncService(profile), GetSigninManager(profile)),
        client->GetPasswordSyncState() ==
            password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE);
  }
  UMA_HISTOGRAM_BOOLEAN("PasswordManager.Enabled", password_manager_enabled);
}

}  // namespace

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
      credentials_filter_(this,
                          base::Bind(&GetSyncService, profile_),
                          base::Bind(&GetSigninManager, profile_)) {
  ContentPasswordManagerDriverFactory::CreateForWebContents(web_contents, this,
                                                            autofill_client);
  driver_factory_ =
      ContentPasswordManagerDriverFactory::FromWebContents(web_contents);
  log_manager_ = password_manager::LogManager::Create(
      password_manager::PasswordManagerInternalsServiceFactory::
          GetForBrowserContext(profile_),
      base::Bind(
          &ContentPasswordManagerDriverFactory::RequestSendLoggingAvailability,
          base::Unretained(driver_factory_)));

  saving_and_filling_passwords_enabled_.Init(
      password_manager::prefs::kPasswordManagerSavingEnabled, GetPrefs());
  ReportMetrics(*saving_and_filling_passwords_enabled_, this, profile_);
  driver_factory_->RequestSendLoggingAvailability();
}

ChromePasswordManagerClient::~ChromePasswordManagerClient() {}

bool ChromePasswordManagerClient::IsAutomaticPasswordSavingEnabled() const {
  return base::FeatureList::IsEnabled(
             password_manager::features::kEnableAutomaticPasswordSaving) &&
         chrome::GetChannel() == version_info::Channel::UNKNOWN;
}

bool ChromePasswordManagerClient::IsPasswordManagementEnabledForCurrentPage()
    const {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  bool is_enabled = false;
  if (!entry) {
    // TODO(gcasto): Determine if fix for crbug.com/388246 is relevant here.
    is_enabled = true;
  } else if (EnabledForSyncSignin()) {
    is_enabled = true;
  } else {
    // Do not fill nor save password when a user is signing in for sync. This
    // is because users need to remember their password if they are syncing as
    // this is effectively their master password.
    is_enabled =
        entry->GetURL().host_piece() != chrome::kChromeUIChromeSigninHost;
  }
  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(
        Logger::STRING_PASSWORD_MANAGEMENT_ENABLED_FOR_CURRENT_PAGE,
        is_enabled);
  }
  return is_enabled;
}

bool ChromePasswordManagerClient::IsSavingAndFillingEnabledForCurrentPage()
    const {
  // TODO(melandory): remove saving_and_filling_passwords_enabled_ check from
  // here once we decide to switch to new settings behavior for everyone.
  return *saving_and_filling_passwords_enabled_ && !IsOffTheRecord() &&
         IsFillingEnabledForCurrentPage();
}

bool ChromePasswordManagerClient::IsFillingEnabledForCurrentPage() const {
  return (!password_manager::IsSettingsBehaviorChangeActive() ||
          *saving_and_filling_passwords_enabled_) &&
         !DidLastPageLoadEncounterSSLErrors() &&
         IsPasswordManagementEnabledForCurrentPage();
}

bool ChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    scoped_ptr<password_manager::PasswordFormManager> form_to_save,
    password_manager::CredentialSourceType type,
    bool update_password) {
  // Save password infobar and the password bubble prompts in case of
  // "webby" URLs and do not prompt in case of "non-webby" URLS (e.g. file://).
  if (!BrowsingDataHelper::IsWebScheme(
      web_contents()->GetLastCommittedURL().scheme())) {
    return false;
  }

  if (IsTheHotNewBubbleUIEnabled()) {
#if !BUILDFLAG(ANDROID_JAVA_UI)
    PasswordsClientUIDelegate* manage_passwords_ui_controller =
        PasswordsClientUIDelegateFromWebContents(web_contents());
    if (update_password && IsUpdatePasswordUIEnabled()) {
      manage_passwords_ui_controller->OnUpdatePasswordSubmitted(
          std::move(form_to_save));
    } else {
      manage_passwords_ui_controller->OnPasswordSubmitted(
          std::move(form_to_save));
    }
#endif
  } else {
#if defined(OS_MACOSX) || BUILDFLAG(ANDROID_JAVA_UI)
    if (form_to_save->IsBlacklisted())
      return false;
    std::string uma_histogram_suffix(
        password_manager::metrics_util::GroupIdToString(
            password_manager::metrics_util::MonitoredDomainGroupId(
                form_to_save->pending_credentials().signon_realm, GetPrefs())));
    SavePasswordInfoBarDelegate::Create(web_contents(), std::move(form_to_save),
                                        uma_histogram_suffix);
#else
    NOTREACHED() << "Aura platforms should always use the bubble";
#endif
  }
  return true;
}

bool ChromePasswordManagerClient::PromptUserToChooseCredentials(
    ScopedVector<autofill::PasswordForm> local_forms,
    ScopedVector<autofill::PasswordForm> federated_forms,
    const GURL& origin,
    base::Callback<void(const password_manager::CredentialInfo&)> callback) {
#if defined(OS_ANDROID)
  // Deletes itself on the event from Java counterpart, when user interacts with
  // dialog.
  AccountChooserDialogAndroid* acccount_chooser_dialog =
      new AccountChooserDialogAndroid(web_contents(), std::move(local_forms),
                                      std::move(federated_forms), origin,
                                      callback);
  acccount_chooser_dialog->ShowDialog();
  return true;
#else
  return PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnChooseCredentials(std::move(local_forms), std::move(federated_forms),
                            origin, callback);
#endif
}

void ChromePasswordManagerClient::ForceSavePassword() {
  password_manager::ContentPasswordManagerDriver* driver =
      driver_factory_->GetDriverForFrame(web_contents()->GetFocusedFrame());
  driver->ForceSavePassword();
}

void ChromePasswordManagerClient::NotifyUserAutoSignin(
    ScopedVector<autofill::PasswordForm> local_forms) {
  DCHECK(!local_forms.empty());
#if BUILDFLAG(ANDROID_JAVA_UI)
  ShowAutoSigninPrompt(web_contents(), local_forms[0]->username_value);
#else
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnAutoSignin(std::move(local_forms));

#endif
}

void ChromePasswordManagerClient::AutomaticPasswordSave(
    scoped_ptr<password_manager::PasswordFormManager> saved_form) {
#if BUILDFLAG(ANDROID_JAVA_UI)
  GeneratedPasswordSavedInfoBarDelegateAndroid::Create(web_contents());
#else
  if (IsTheHotNewBubbleUIEnabled()) {
    PasswordsClientUIDelegate* manage_passwords_ui_controller =
        PasswordsClientUIDelegateFromWebContents(web_contents());
    manage_passwords_ui_controller->OnAutomaticPasswordSave(
        std::move(saved_form));
  }
#endif
}

void ChromePasswordManagerClient::PasswordWasAutofilled(
    const autofill::PasswordFormMap& best_matches,
    const GURL& origin) const {
#if !BUILDFLAG(ANDROID_JAVA_UI)
  PasswordsClientUIDelegate* manage_passwords_ui_controller =
      PasswordsClientUIDelegateFromWebContents(web_contents());
  if (manage_passwords_ui_controller && IsTheHotNewBubbleUIEnabled())
    manage_passwords_ui_controller->OnPasswordAutofilled(best_matches, origin);
#endif
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

password_manager::PasswordSyncState
ChromePasswordManagerClient::GetPasswordSyncState() const {
  const ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return password_manager_util::GetPasswordSyncState(sync_service);
}

bool ChromePasswordManagerClient::WasLastNavigationHTTPError() const {
  DCHECK(web_contents());

  scoped_ptr<password_manager::BrowserSavePasswordProgressLogger> logger;
  if (log_manager_->IsLoggingActive()) {
    logger.reset(new password_manager::BrowserSavePasswordProgressLogger(
        log_manager_.get()));
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
  bool ssl_errors = true;
  if (!entry) {
    ssl_errors = false;
  } else {
    ssl_errors = net::IsCertStatusError(entry->GetSSL().cert_status);
  }
  if (log_manager_->IsLoggingActive()) {
    password_manager::BrowserSavePasswordProgressLogger logger(
        log_manager_.get());
    logger.LogBoolean(Logger::STRING_SSL_ERRORS_PRESENT, ssl_errors);
  }
  return ssl_errors;
}

bool ChromePasswordManagerClient::IsOffTheRecord() const {
  return web_contents()->GetBrowserContext()->IsOffTheRecord();
}

const password_manager::PasswordManager*
ChromePasswordManagerClient::GetPasswordManager() const {
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
    IPC_MESSAGE_HANDLER(AutofillHostMsg_GenerationAvailableForForm,
                        GenerationAvailableForForm)
    // Default:
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void ChromePasswordManagerClient::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Logging has no sense on WebUI sites.
  log_manager_->SetSuspended(web_contents()->GetWebUI() != nullptr);
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

void ChromePasswordManagerClient::PromptUserToEnableAutosignin() {
#if BUILDFLAG(ANDROID_JAVA_UI)
  // TODO(crbug.com/532876): pop up the dialog.
#else
  PasswordsClientUIDelegateFromWebContents(web_contents())
      ->OnPromptEnableAutoSignin();
#endif
}

void ChromePasswordManagerClient::GenerationAvailableForForm(
    const autofill::PasswordForm& form) {
  password_manager_.GenerationAvailableForForm(form);
}

bool ChromePasswordManagerClient::IsTheHotNewBubbleUIEnabled() {
#if BUILDFLAG(ANDROID_JAVA_UI)
  return false;
#elif defined(OS_MACOSX)
  // Query the group first for correct UMA reporting.
  std::string group_name =
      base::FieldTrialList::FindFullName("PasswordManagerUI");
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDisableSavePasswordBubble))
    return false;

  if (command_line->HasSwitch(switches::kEnableSavePasswordBubble))
    return true;

  // The bubble should be the default case that runs on the bots.
  return group_name != "Infobar";
#else
  // All other platforms use Aura, and therefore always show the bubble.
  return true;
#endif
}

bool ChromePasswordManagerClient::IsUpdatePasswordUIEnabled() const {
  // Currently Password update UI is implemented only for Bubble UI.
  return IsTheHotNewBubbleUIEnabled();
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

const GURL& ChromePasswordManagerClient::GetMainFrameURL() const {
  return web_contents()->GetVisibleURL();
}

const GURL& ChromePasswordManagerClient::GetLastCommittedEntryURL() const {
  DCHECK(web_contents());
  content::NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  if (!entry)
    return GURL::EmptyGURL();

  return entry->GetURL();
}

const password_manager::CredentialsFilter*
ChromePasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

const password_manager::LogManager* ChromePasswordManagerClient::GetLogManager()
    const {
  return log_manager_.get();
}
