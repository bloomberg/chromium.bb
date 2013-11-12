// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/password_manager_presenter.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/password_manager/password_manager_util.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/password/password_ui_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "content/public/browser/user_metrics.h"

namespace options {

PasswordManagerPresenter::PasswordManagerPresenter(
    passwords_ui::PasswordUIView* password_view)
    : populater_(this),
      exception_populater_(this),
      password_view_(password_view) {
  DCHECK(password_view_);
  require_reauthentication_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePasswordManagerReauthentication);
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
      prefs::kPasswordManagerAllowShowPasswords,
      password_view_->GetProfile()->GetPrefs(),
      base::Bind(&PasswordManagerPresenter::UpdatePasswordLists,
                 base::Unretained(this)));
  // TODO(jhawkins) We should not cache web_ui()->GetProfile().See
  // crosbug.com/6304.
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->AddObserver(this);
}

void PasswordManagerPresenter::OnLoginsChanged() {
  UpdatePasswordLists();
}

PasswordStore* PasswordManagerPresenter::GetPasswordStore() {
  return PasswordStoreFactory::GetForProfile(password_view_->GetProfile(),
                                             Profile::EXPLICIT_ACCESS).get();
}

void PasswordManagerPresenter::UpdatePasswordLists() {
  // Reset the current lists.
  password_list_.clear();
  password_exception_list_.clear();

  populater_.Populate();
  exception_populater_.Populate();
}

void PasswordManagerPresenter::HandleRemoveSavedPassword(size_t index) {
  DCHECK_LT(index, password_list_.size());
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  store->RemoveLogin(*password_list_[index]);
  content::RecordAction(
      content::UserMetricsAction("PasswordManager_HandleRemoveSavedPassword"));
}

void PasswordManagerPresenter::HandleRemovePasswordException(size_t index) {
  DCHECK_LT(index, password_exception_list_.size());
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;
  store->RemoveLogin(*password_exception_list_[index]);
  content::RecordAction(
      content::UserMetricsAction(
          "PasswordManager_HandleRemovePasswordException"));
}

void PasswordManagerPresenter::HandleRequestShowPassword(size_t index) {
  DCHECK_LT(index, password_list_.size());
  if (IsAuthenticationRequired()) {
    if (password_manager_util::AuthenticateUser())
      last_authentication_time_ = base::TimeTicks::Now();
    else
      return;
  }
  // Call back the front end to reveal the password.
  password_view_->ShowPassword(index, password_list_[index]->password_value);
}

const autofill::PasswordForm& PasswordManagerPresenter::GetPassword(
    size_t index) {
  DCHECK_LT(index, password_list_.size());
  return *password_list_[index];
}

const autofill::PasswordForm& PasswordManagerPresenter::GetPasswordException(
    size_t index) {
  DCHECK_LT(index, password_exception_list_.size());
  return *password_exception_list_[index];
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
    PasswordManagerPresenter* page)
    : page_(page),
      pending_login_query_(0) {
}

PasswordManagerPresenter::ListPopulater::~ListPopulater() {
}

PasswordManagerPresenter::PasswordListPopulater::PasswordListPopulater(
    PasswordManagerPresenter* page) : ListPopulater(page) {
}

void PasswordManagerPresenter::PasswordListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    if (pending_login_query_)
      store->CancelRequest(pending_login_query_);

    pending_login_query_ = store->GetAutofillableLogins(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display passwords.";
  }
}

void PasswordManagerPresenter::PasswordListPopulater::
    OnPasswordStoreRequestDone(
        CancelableRequestProvider::Handle handle,
        const std::vector<autofill::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_list_.clear();
  page_->password_list_.insert(page_->password_list_.end(),
                               result.begin(), result.end());
  page_->SetPasswordList();
}

void PasswordManagerPresenter::PasswordListPopulater::OnGetPasswordStoreResults(
    const std::vector<autofill::PasswordForm*>& results) {
  // TODO(kaiwang): Implement when I refactor
  // PasswordStore::GetAutofillableLogins and PasswordStore::GetBlacklistLogins.
  NOTIMPLEMENTED();
}

PasswordManagerPresenter::PasswordExceptionListPopulater::
    PasswordExceptionListPopulater(PasswordManagerPresenter* page)
        : ListPopulater(page) {
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    if (pending_login_query_)
      store->CancelRequest(pending_login_query_);

    pending_login_query_ = store->GetBlacklistLogins(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display exceptions.";
  }
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::
    OnPasswordStoreRequestDone(
        CancelableRequestProvider::Handle handle,
        const std::vector<autofill::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->password_exception_list_.clear();
  page_->password_exception_list_.insert(page_->password_exception_list_.end(),
                                         result.begin(), result.end());
  page_->SetPasswordExceptionList();
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::
    OnGetPasswordStoreResults(
        const std::vector<autofill::PasswordForm*>& results) {
  // TODO(kaiwang): Implement when I refactor
  // PasswordStore::GetAutofillableLogins and PasswordStore::GetBlacklistLogins.
  NOTIMPLEMENTED();
}

}  // namespace options
