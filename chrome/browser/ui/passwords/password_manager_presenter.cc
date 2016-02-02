// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/sync/browser/password_sync_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

using password_manager::PasswordStore;

PasswordManagerPresenter::PasswordManagerPresenter(
    PasswordUIView* password_view)
    : populater_(this),
      exception_populater_(this),
      require_reauthentication_(
          !base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kDisablePasswordManagerReauthentication)),
      password_view_(password_view) {
  DCHECK(password_view_);
}

PasswordManagerPresenter::~PasswordManagerPresenter() {
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->RemoveObserver(this);
}

void PasswordManagerPresenter::Initialize() {
  // Due to the way that handlers are (re)initialized under certain types of
  // navigation, the presenter may already be initialized. (See bugs 88986
  // and 86448). If this is the case, return immediately. This is a hack.
  // TODO(mdm): remove this hack once it is no longer necessary.
  if (!show_passwords_.GetPrefName().empty())
    return;

  show_passwords_.Init(
      password_manager::prefs::kPasswordManagerAllowShowPasswords,
      password_view_->GetProfile()->GetPrefs(),
      base::Bind(&PasswordManagerPresenter::UpdatePasswordLists,
                 base::Unretained(this)));
  // TODO(jhawkins) We should not cache web_ui()->GetProfile().See
  // crosbug.com/6304.
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->AddObserver(this);

  languages_ = password_view_->GetProfile()->GetPrefs()->
      GetString(prefs::kAcceptLanguages);
}

void PasswordManagerPresenter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Entire list is updated for convenience.
  UpdatePasswordLists();
}

PasswordStore* PasswordManagerPresenter::GetPasswordStore() {
  return PasswordStoreFactory::GetForProfile(password_view_->GetProfile(),
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}

void PasswordManagerPresenter::UpdatePasswordLists() {
  // Reset so that showing a password will require re-authentication.
  last_authentication_time_ = base::TimeTicks();

  // Reset the current lists.
  password_list_.clear();
  password_exception_list_.clear();

  populater_.Populate();
  exception_populater_.Populate();
}

void PasswordManagerPresenter::RemoveSavedPassword(size_t index) {
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return;
  }
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  store->RemoveLogin(*password_list_[index]);
  content::RecordAction(
      base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
}

void PasswordManagerPresenter::RemovePasswordException(size_t index) {
  if (index >= password_exception_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return;
  }
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  store->RemoveLogin(*password_exception_list_[index]);
  content::RecordAction(
      base::UserMetricsAction("PasswordManager_RemovePasswordException"));
}

void PasswordManagerPresenter::RequestShowPassword(size_t index) {
#if !defined(OS_ANDROID)  // This is never called on Android.
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return;
  }
  if (require_reauthentication_ &&
      (base::TimeTicks::Now() - last_authentication_time_) >
          base::TimeDelta::FromSeconds(60)) {
    bool authenticated = true;
#if defined(OS_WIN)
    authenticated = password_manager_util_win::AuthenticateUser(
        password_view_->GetNativeWindow());
#elif defined(OS_MACOSX)
    authenticated = password_manager_util_mac::AuthenticateUser();
#endif
    if (authenticated)
      last_authentication_time_ = base::TimeTicks::Now();
    else
      return;
  }

  sync_driver::SyncService* sync_service = nullptr;
  if (ProfileSyncServiceFactory::HasProfileSyncService(
          password_view_->GetProfile())) {
    sync_service =
        ProfileSyncServiceFactory::GetForProfile(password_view_->GetProfile());
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          *password_list_[index], sync_service,
          SigninManagerFactory::GetForProfile(password_view_->GetProfile()))) {
    content::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  // Call back the front end to reveal the password.
  std::string origin_url = password_manager::GetHumanReadableOrigin(
      *password_list_[index], languages_);
  password_view_->ShowPassword(
      index,
      origin_url,
      base::UTF16ToUTF8(password_list_[index]->username_value),
      password_list_[index]->password_value);
#endif
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPassword(
    size_t index) {
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return NULL;
  }
  return password_list_[index].get();
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPasswordException(
    size_t index) {
  if (index >= password_exception_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return NULL;
  }
  return password_exception_list_[index].get();
}

void PasswordManagerPresenter::SetPasswordList() {
  // Due to the way that handlers are (re)initialized under certain types of
  // navigation, the presenter may already be initialized. (See bugs 88986
  // and 86448). If this is the case, return immediately. This is a hack.
  // If this is the case, initialize on demand. This is a hack.
  // TODO(mdm): remove this hack once it is no longer necessary.
  if (show_passwords_.GetPrefName().empty())
    Initialize();

  bool show_passwords = *show_passwords_ && !require_reauthentication_;
  password_view_->SetPasswordList(password_list_, show_passwords);
}

void PasswordManagerPresenter::SetPasswordExceptionList() {
  password_view_->SetPasswordExceptionList(password_exception_list_);
}

PasswordManagerPresenter::ListPopulater::ListPopulater(
    PasswordManagerPresenter* page) : page_(page) {
}

PasswordManagerPresenter::ListPopulater::~ListPopulater() {
}

PasswordManagerPresenter::PasswordListPopulater::PasswordListPopulater(
    PasswordManagerPresenter* page) : ListPopulater(page) {
}

void PasswordManagerPresenter::PasswordListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    cancelable_task_tracker()->TryCancelAll();
    store->GetAutofillableLogins(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display passwords.";
  }
}

void PasswordManagerPresenter::PasswordListPopulater::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  page_->password_list_ =
      password_manager_util::ConvertScopedVector(std::move(results));
  page_->SetPasswordList();
}

PasswordManagerPresenter::PasswordExceptionListPopulater::
    PasswordExceptionListPopulater(PasswordManagerPresenter* page)
        : ListPopulater(page) {
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    cancelable_task_tracker()->TryCancelAll();
    store->GetBlacklistLogins(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display exceptions.";
  }
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::
    OnGetPasswordStoreResults(ScopedVector<autofill::PasswordForm> results) {
  page_->password_exception_list_ =
      password_manager_util::ConvertScopedVector(std::move(results));
  page_->SetPasswordExceptionList();
}
