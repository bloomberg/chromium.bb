// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ios_chrome_password_manager_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "components/autofill/core/common/password_form.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_requirements_service.h"
#include "components/password_manager/core/browser/store_metrics_reporter.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/translate/core/browser/translate_manager.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/credential_manager_util.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#include "ios/chrome/browser/passwords/ios_password_requirements_service_factory.h"
#include "ios/chrome/browser/passwords/password_manager_internals_service_factory.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/system_flags.h"
#include "ios/chrome/browser/translate/chrome_ios_translate_client.h"
#include "net/cert/cert_status_flags.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::PasswordFormManagerForUI;
using password_manager::PasswordManagerMetricsRecorder;
using password_manager::PasswordStore;
using password_manager::SyncState;

namespace {

const syncer::SyncService* GetSyncService(
    ios::ChromeBrowserState* browser_state) {
  return ProfileSyncServiceFactory::GetForBrowserStateIfExists(browser_state);
}

const identity::IdentityManager* GetIdentityManager(
    ios::ChromeBrowserState* browser_state) {
  return IdentityManagerFactory::GetForBrowserState(browser_state);
}

}  // namespace

IOSChromePasswordManagerClient::IOSChromePasswordManagerClient(
    id<PasswordManagerClientDelegate> delegate)
    : delegate_(delegate),
      credentials_filter_(
          this,
          base::BindRepeating(&GetSyncService, delegate_.browserState),
          base::BindRepeating(&GetIdentityManager, delegate_.browserState)),
      helper_(this) {
  saving_passwords_enabled_.Init(
      password_manager::prefs::kCredentialsEnableService, GetPrefs());
  static base::NoDestructor<password_manager::StoreMetricsReporter> reporter(
      *saving_passwords_enabled_, this, GetSyncService(delegate_.browserState),
      GetIdentityManager(delegate_.browserState), GetPrefs());
  log_manager_ = password_manager::LogManager::Create(
      ios::PasswordManagerInternalsServiceFactory::GetForBrowserState(
          delegate_.browserState),
      base::Closure());
}

IOSChromePasswordManagerClient::~IOSChromePasswordManagerClient() = default;

SyncState IOSChromePasswordManagerClient::GetPasswordSyncState() const {
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForBrowserState(delegate_.browserState);
  return password_manager_util::GetPasswordSyncState(sync_service);
}

bool IOSChromePasswordManagerClient::PromptUserToChooseCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin,
    const CredentialsCallback& callback) {
  NOTIMPLEMENTED();
  return false;
}

bool IOSChromePasswordManagerClient::PromptUserToSaveOrUpdatePassword(
    std::unique_ptr<PasswordFormManagerForUI> form_to_save,
    bool update_password) {
  if (form_to_save->IsBlacklisted())
    return false;

  if (update_password) {
    [delegate_ showUpdatePasswordInfoBar:std::move(form_to_save)];
  } else {
    [delegate_ showSavePasswordInfoBar:std::move(form_to_save)];
  }

  return true;
}

void IOSChromePasswordManagerClient::ShowManualFallbackForSaving(
    std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
    bool has_generated_password,
    bool is_update) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerClient::HideManualFallbackForSaving() {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerClient::FocusedInputChanged(
    const url::Origin& last_committed_origin,
    bool is_fillable,
    bool is_password_field) {
  NOTIMPLEMENTED();
}

void IOSChromePasswordManagerClient::AutomaticPasswordSave(
    std::unique_ptr<PasswordFormManagerForUI> saved_form_manager) {
  NOTIMPLEMENTED();
}

bool IOSChromePasswordManagerClient::IsIncognito() const {
  return (delegate_.browserState)->IsOffTheRecord();
}

const password_manager::PasswordManager*
IOSChromePasswordManagerClient::GetPasswordManager() const {
  return delegate_.passwordManager;
}

bool IOSChromePasswordManagerClient::IsMainFrameSecure() const {
  return WebStateContentIsSecureHtml(delegate_.webState);
}

PrefService* IOSChromePasswordManagerClient::GetPrefs() const {
  return (delegate_.browserState)->GetPrefs();
}

PasswordStore* IOSChromePasswordManagerClient::GetPasswordStore() const {
  return IOSChromePasswordStoreFactory::GetForBrowserState(
             delegate_.browserState, ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

void IOSChromePasswordManagerClient::NotifyUserAutoSignin(
    std::vector<std::unique_ptr<autofill::PasswordForm>> local_forms,
    const GURL& origin) {
  DCHECK(!local_forms.empty());
  helper_.NotifyUserAutoSignin();
  [delegate_ showAutosigninNotification:std::move(local_forms[0])];
}

void IOSChromePasswordManagerClient::NotifyUserCouldBeAutoSignedIn(
    std::unique_ptr<autofill::PasswordForm> form) {
  helper_.NotifyUserCouldBeAutoSignedIn(std::move(form));
}

void IOSChromePasswordManagerClient::NotifySuccessfulLoginWithExistingPassword(
    const autofill::PasswordForm& form) {
  helper_.NotifySuccessfulLoginWithExistingPassword(form);
}

void IOSChromePasswordManagerClient::NotifyStorePasswordCalled() {
  helper_.NotifyStorePasswordCalled();
}

bool IOSChromePasswordManagerClient::IsSavingAndFillingEnabled(
    const GURL& url) const {
  return *saving_passwords_enabled_ && !IsIncognito() &&
         !net::IsCertStatusError(GetMainFrameCertStatus()) &&
         IsFillingEnabled(url);
}

bool IOSChromePasswordManagerClient::IsFillingEnabled(const GURL& url) const {
  return url.GetOrigin() !=
         GURL(password_manager::kPasswordManagerAccountDashboardURL);
}

const GURL& IOSChromePasswordManagerClient::GetLastCommittedEntryURL() const {
  return delegate_.lastCommittedURL;
}

std::string IOSChromePasswordManagerClient::GetPageLanguage() const {
  // TODO(crbug.com/912597): Add WebState to the IOSChromePasswordManagerClient
  // to be able to get the pages LanguageState from the TranslateManager.
  return std::string();
}

const password_manager::CredentialsFilter*
IOSChromePasswordManagerClient::GetStoreResultFilter() const {
  return &credentials_filter_;
}

const password_manager::LogManager*
IOSChromePasswordManagerClient::GetLogManager() const {
  return log_manager_.get();
}

ukm::SourceId IOSChromePasswordManagerClient::GetUkmSourceId() {
  return delegate_.ukmSourceId;
}

PasswordManagerMetricsRecorder*
IOSChromePasswordManagerClient::GetMetricsRecorder() {
  if (!metrics_recorder_) {
    metrics_recorder_.emplace(GetUkmSourceId(), delegate_.lastCommittedURL);
  }
  return base::OptionalOrNullptr(metrics_recorder_);
}

password_manager::PasswordRequirementsService*
IOSChromePasswordManagerClient::GetPasswordRequirementsService() {
  return IOSPasswordRequirementsServiceFactory::GetForBrowserState(
      delegate_.browserState, ServiceAccessType::EXPLICIT_ACCESS);
}

void IOSChromePasswordManagerClient::PromptUserToEnableAutosignin() {
  // TODO(crbug.com/435048): Implement this method.
}

password_manager::PasswordManager*
IOSChromePasswordManagerClient::GetPasswordManager() {
  return delegate_.passwordManager;
}

bool IOSChromePasswordManagerClient::IsIsolationForPasswordSitesEnabled()
    const {
  return false;
}
