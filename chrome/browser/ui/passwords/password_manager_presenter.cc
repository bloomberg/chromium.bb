// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/user_metrics_action.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/password_manager/password_manager_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/password_manager/sync_metrics.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"

using password_manager::PasswordStore;

PasswordManagerPresenter::PasswordManagerPresenter(
    PasswordUIView* password_view)
    : populater_(this),
      exception_populater_(this),
      password_view_(password_view) {
  DCHECK(password_view_);
  require_reauthentication_ = !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisablePasswordManagerReauthentication);
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
}

void PasswordManagerPresenter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Entire list is updated for convenience.
  UpdatePasswordLists();
}

PasswordStore* PasswordManagerPresenter::GetPasswordStore() {
  return PasswordStoreFactory::GetForProfile(password_view_->GetProfile(),
                                             Profile::EXPLICIT_ACCESS).get();
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
#if !defined(OS_ANDROID) // This is never called on Android.
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return;
  }
  if (IsAuthenticationRequired()) {
    if (password_manager_util::AuthenticateUser(
        password_view_->GetNativeWindow()))
      last_authentication_time_ = base::TimeTicks::Now();
    else
      return;
  }

  if (password_manager_sync_metrics::IsSyncAccountCredential(
          password_view_->GetProfile(),
          base::UTF16ToUTF8(password_list_[index]->username_value),
          password_list_[index]->signon_realm)) {
    content::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  // Call back the front end to reveal the password.
  password_view_->ShowPassword(index, password_list_[index]->password_value);
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
  return password_list_[index];
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPasswordException(
    size_t index) {
  if (index >= password_exception_list_.size()) {
    // |index| out of bounds might come from a compromised renderer, don't let
    // it crash the browser. http://crbug.com/362054
    NOTREACHED();
    return NULL;
  }
  return password_exception_list_[index];
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

bool PasswordManagerPresenter::IsAuthenticationRequired() {
  base::TimeDelta delta = base::TimeDelta::FromSeconds(60);
  return require_reauthentication_ &&
      (base::TimeTicks::Now() - last_authentication_time_) > delta;
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
    const std::vector<autofill::PasswordForm*>& results) {
  page_->password_list_.clear();
  page_->password_list_.insert(page_->password_list_.end(),
                               results.begin(), results.end());
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
    OnGetPasswordStoreResults(
        const std::vector<autofill::PasswordForm*>& results) {
  page_->password_exception_list_.clear();
  page_->password_exception_list_.insert(page_->password_exception_list_.end(),
                                         results.begin(), results.end());
  page_->SetPasswordExceptionList();
}
