// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/settings/password_manager_presenter.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/settings/password_ui_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_list_sorter.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service.h"
#include "components/undo/undo_operation.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#endif

using base::StringPiece;
using password_manager::PasswordStore;

namespace {

// Convenience typedef for the commonly used vector of PasswordForm pointers.
using FormVector = std::vector<std::unique_ptr<autofill::PasswordForm>>;

const autofill::PasswordForm* TryGetPasswordForm(
    const std::map<std::string, FormVector>& map,
    size_t index) {
  // |index| out of bounds might come from a compromised renderer
  // (http://crbug.com/362054), or the user removed a password while a request
  // to the store is in progress (i.e. |forms| is empty). Don't let it crash
  // the browser.
  if (map.size() <= index)
    return nullptr;

  // Android tries to obtain a PasswordForm corresponding to a specific index,
  // and does not know about sort keys. In order to efficiently obtain the n'th
  // element in the map we make use of std::next() here.
  const auto& forms = std::next(map.begin(), index)->second;
  DCHECK(!forms.empty());
  return forms[0].get();
}

// Processes |map| and returns a FormVector where each equivalence class in
// |map| is represented by exactly one entry in the result.
FormVector GetEntryList(const std::map<std::string, FormVector>& map) {
  FormVector result;
  result.reserve(map.size());
  for (const auto& pair : map) {
    DCHECK(!pair.second.empty());
    result.push_back(std::make_unique<autofill::PasswordForm>(*pair.second[0]));
  }

  return result;
}

class RemovePasswordOperation : public UndoOperation {
 public:
  RemovePasswordOperation(PasswordManagerPresenter* page,
                          const autofill::PasswordForm& form);
  ~RemovePasswordOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  PasswordManagerPresenter* page_;
  autofill::PasswordForm form_;

  DISALLOW_COPY_AND_ASSIGN(RemovePasswordOperation);
};

RemovePasswordOperation::RemovePasswordOperation(
    PasswordManagerPresenter* page,
    const autofill::PasswordForm& form)
    : page_(page), form_(form) {}

RemovePasswordOperation::~RemovePasswordOperation() = default;

void RemovePasswordOperation::Undo() {
  page_->AddLogin(form_);
}

int RemovePasswordOperation::GetUndoLabelId() const {
  return 0;
}

int RemovePasswordOperation::GetRedoLabelId() const {
  return 0;
}

class AddPasswordOperation : public UndoOperation {
 public:
  AddPasswordOperation(PasswordManagerPresenter* page,
                       const autofill::PasswordForm& password_form);
  ~AddPasswordOperation() override;

  // UndoOperation:
  void Undo() override;
  int GetUndoLabelId() const override;
  int GetRedoLabelId() const override;

 private:
  PasswordManagerPresenter* page_;
  autofill::PasswordForm form_;

  DISALLOW_COPY_AND_ASSIGN(AddPasswordOperation);
};

AddPasswordOperation::AddPasswordOperation(PasswordManagerPresenter* page,
                                           const autofill::PasswordForm& form)
    : page_(page), form_(form) {}

AddPasswordOperation::~AddPasswordOperation() = default;

void AddPasswordOperation::Undo() {
  page_->RemoveLogin(form_);
}

int AddPasswordOperation::GetUndoLabelId() const {
  return 0;
}

int AddPasswordOperation::GetRedoLabelId() const {
  return 0;
}

}  // namespace

PasswordManagerPresenter::PasswordManagerPresenter(
    PasswordUIView* password_view)
    : password_view_(password_view) {
  DCHECK(password_view_);
}

PasswordManagerPresenter::~PasswordManagerPresenter() {
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->RemoveObserver(this);
}

void PasswordManagerPresenter::Initialize() {
  PasswordStore* store = GetPasswordStore();
  if (store)
    store->AddObserver(this);
}

void PasswordManagerPresenter::OnLoginsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Entire maps are updated for convenience.
  UpdatePasswordLists();
}

void PasswordManagerPresenter::UpdatePasswordLists() {
  // Reset the current maps.
  password_map_.clear();
  exception_map_.clear();

  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  CancelAllRequests();
  store->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPassword(
    size_t index) const {
  return TryGetPasswordForm(password_map_, index);
}

FormVector PasswordManagerPresenter::GetAllPasswords() {
  FormVector ret_val;
  for (const auto& pair : password_map_) {
    for (const auto& form : pair.second) {
      ret_val.push_back(std::make_unique<autofill::PasswordForm>(*form));
    }
  }

  return ret_val;
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPasswordException(
    size_t index) const {
  return TryGetPasswordForm(exception_map_, index);
}

void PasswordManagerPresenter::ChangeSavedPassword(
    size_t index,
    const base::string16& new_username,
    const base::Optional<base::string16>& new_password) {
  DCHECK_LT(index, password_map_.size());

  ChangeSavedPasswords(std::next(password_map_.begin(), index)->second,
                       new_username, new_password);
}

void PasswordManagerPresenter::ChangeSavedPassword(
    const std::string& sort_key,
    const base::string16& new_username,
    const base::Optional<base::string16>& new_password) {
  // Find the equivalence class that needs to be updated.
  auto it = password_map_.find(sort_key);
  if (it == password_map_.end())
    return;

  ChangeSavedPasswords(it->second, new_username, new_password);
}

void PasswordManagerPresenter::RemoveSavedPassword(size_t index) {
  if (TryRemovePasswordEntries(&password_map_, index)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordManagerPresenter::RemoveSavedPassword(
    const std::string& sort_key) {
  if (TryRemovePasswordEntries(&password_map_, sort_key)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordManagerPresenter::RemovePasswordException(size_t index) {
  if (TryRemovePasswordEntries(&exception_map_, index)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  }
}

void PasswordManagerPresenter::RemovePasswordException(
    const std::string& sort_key) {
  if (TryRemovePasswordEntries(&exception_map_, sort_key)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  }
}

void PasswordManagerPresenter::UndoRemoveSavedPasswordOrException() {
  undo_manager_.Undo();
}

#if !defined(OS_ANDROID)  // This is never called on Android.
void PasswordManagerPresenter::RequestShowPassword(
    const std::string& sort_key,
    base::OnceCallback<void(base::Optional<base::string16>)> callback) const {
  auto it = password_map_.find(sort_key);
  if (it == password_map_.end()) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  DCHECK(!it->second.empty());
  const auto& form = *it->second[0];
  syncer::SyncService* sync_service = nullptr;
  if (ProfileSyncServiceFactory::HasSyncService(password_view_->GetProfile())) {
    sync_service =
        ProfileSyncServiceFactory::GetForProfile(password_view_->GetProfile());
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          form, sync_service,
          IdentityManagerFactory::GetForProfile(
              password_view_->GetProfile()))) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  // Call back the front end to reveal the password.
  std::move(callback).Run(form.password_value);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
}
#endif

void PasswordManagerPresenter::AddLogin(const autofill::PasswordForm& form) {
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  undo_manager_.AddUndoOperation(
      std::make_unique<AddPasswordOperation>(this, form));
  store->AddLogin(form);
}

void PasswordManagerPresenter::RemoveLogin(const autofill::PasswordForm& form) {
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  undo_manager_.AddUndoOperation(
      std::make_unique<RemovePasswordOperation>(this, form));
  store->RemoveLogin(form);
}

void PasswordManagerPresenter::ChangeSavedPasswords(
    const FormVector& old_forms,
    const base::string16& new_username,
    const base::Optional<base::string16>& new_password) {
  // If a password was provided, make sure it is not empty.
  if (new_password && new_password->empty()) {
    DLOG(ERROR) << "The password is empty.";
    return;
  }

  DCHECK(!old_forms.empty());
  const std::string& signon_realm = old_forms[0]->signon_realm;
  const base::string16& old_username = old_forms[0]->username_value;

  const bool username_changed = old_username != new_username;
  // In case the username changed, make sure that there exists no other
  // credential with the same signon_realm and username.
  if (username_changed) {
    for (const auto& sort_key_passwords_pair : password_map_) {
      for (const auto& password : sort_key_passwords_pair.second) {
        if (password->signon_realm == signon_realm &&
            password->username_value == new_username) {
          DLOG(ERROR) << "A credential with the same signon_realm and username "
                         "already exists.";
          return;
        }
      }
    }
  }

  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  // An updated username implies a change in the primary key, thus we need to
  // make sure to call the right API. Update every entry in the equivalence
  // class.
  for (const auto& old_form : old_forms) {
    autofill::PasswordForm new_form = *old_form;
    new_form.username_value = new_username;
    if (new_password)
      new_form.password_value = *new_password;

    username_changed ? store->UpdateLoginWithPrimaryKey(new_form, *old_form)
                     : store->UpdateLogin(new_form);
  }
}

bool PasswordManagerPresenter::TryRemovePasswordEntries(
    PasswordFormMap* form_map,
    size_t index) {
  if (form_map->size() <= index)
    return false;

  // Android tries to obtain a PasswordForm corresponding to a specific index,
  // and does not know about sort keys. In order to efficiently obtain the n'th
  // element in the map we make use of std::next() here.
  return TryRemovePasswordEntries(form_map,
                                  std::next(form_map->cbegin(), index));
}

bool PasswordManagerPresenter::TryRemovePasswordEntries(
    PasswordFormMap* form_map,
    const std::string& sort_key) {
  auto it = form_map->find(sort_key);
  if (it == form_map->end())
    return false;

  return TryRemovePasswordEntries(form_map, it);
}

bool PasswordManagerPresenter::TryRemovePasswordEntries(
    PasswordFormMap* form_map,
    PasswordFormMap::const_iterator forms_iter) {
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return false;

  const FormVector& forms = forms_iter->second;
  DCHECK(!forms.empty());

  undo_manager_.AddUndoOperation(
      std::make_unique<RemovePasswordOperation>(this, *forms[0]));
  for (const auto& form : forms)
    store->RemoveLogin(*form);

  form_map->erase(forms_iter);
  return true;
}

void PasswordManagerPresenter::OnGetPasswordStoreResults(FormVector results) {
  for (auto& form : results) {
    auto& form_map = form->blacklisted_by_user ? exception_map_ : password_map_;
    form_map[password_manager::CreateSortKey(*form)].push_back(std::move(form));
  }

  SetPasswordList();
  SetPasswordExceptionList();
}

void PasswordManagerPresenter::SetPasswordList() {
  // TODO(https://crbug.com/892260): Creating a copy of the elements is
  // wasteful. Consider updating PasswordUIView to take a PasswordFormMap
  // instead.
  password_view_->SetPasswordList(GetEntryList(password_map_));
}

void PasswordManagerPresenter::SetPasswordExceptionList() {
  // TODO(https://crbug.com/892260): Creating a copy of the elements is
  // wasteful. Consider updating PasswordUIView to take a PasswordFormMap
  // instead.
  password_view_->SetPasswordExceptionList(GetEntryList(exception_map_));
}

PasswordStore* PasswordManagerPresenter::GetPasswordStore() {
  return PasswordStoreFactory::GetForProfile(password_view_->GetProfile(),
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}
