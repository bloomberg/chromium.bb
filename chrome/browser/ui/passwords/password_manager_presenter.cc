// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"
#include "chrome/browser/ui/passwords/password_ui_view.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/common/password_form.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/undo/undo_operation.h"
#include "content/public/browser/browser_thread.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#endif

using base::StringPiece;
using password_manager::PasswordStore;

namespace {

// Finds duplicates of |form| in |duplicates|, removes them from |store| and
// from |duplicates|.
void RemoveDuplicates(const autofill::PasswordForm& form,
                      password_manager::DuplicatesMap* duplicates,
                      PasswordStore* store) {
  std::string key = password_manager::CreateSortKey(form);
  std::pair<password_manager::DuplicatesMap::iterator,
            password_manager::DuplicatesMap::iterator>
      dups = duplicates->equal_range(key);
  for (auto it = dups.first; it != dups.second; ++it)
    store->RemoveLogin(*it->second);
  duplicates->erase(key);
}

const autofill::PasswordForm* TryGetPasswordForm(
    const std::vector<std::unique_ptr<autofill::PasswordForm>>& forms,
    size_t index) {
  // |index| out of bounds might come from a compromised renderer
  // (http://crbug.com/362054), or the user removed a password while a request
  // to the store is in progress (i.e. |forms| is empty). Don't let it crash
  // the browser.
  return index < forms.size() ? forms[index].get() : nullptr;
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
  // Entire list is updated for convenience.
  UpdatePasswordLists();
}

void PasswordManagerPresenter::UpdatePasswordLists() {
  // Reset the current lists.
  password_list_.clear();
  password_duplicates_.clear();
  password_exception_list_.clear();
  password_exception_duplicates_.clear();

  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  CancelAllRequests();
  store->GetAllLoginsWithAffiliationAndBrandingInformation(this);
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPassword(
    size_t index) {
  return TryGetPasswordForm(password_list_, index);
}

std::vector<std::unique_ptr<autofill::PasswordForm>>
PasswordManagerPresenter::GetAllPasswords() {
  std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;

  for (const auto& form : password_list_) {
    ret_val.push_back(std::make_unique<autofill::PasswordForm>(*form));
  }

  return ret_val;
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPasswordException(
    size_t index) {
  return TryGetPasswordForm(password_exception_list_, index);
}

void PasswordManagerPresenter::RemoveSavedPassword(size_t index) {
  if (TryRemovePasswordEntry(EntryKind::kPassword, index)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
  }
}

void PasswordManagerPresenter::RemovePasswordException(size_t index) {
  if (TryRemovePasswordEntry(EntryKind::kException, index)) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_RemovePasswordException"));
  }
}

void PasswordManagerPresenter::UndoRemoveSavedPasswordOrException() {
  undo_manager_.Undo();
}

void PasswordManagerPresenter::RequestShowPassword(size_t index) {
#if !defined(OS_ANDROID)  // This is never called on Android.
  const auto* form = TryGetPasswordForm(password_list_, index);
  if (!form)
    return;

  syncer::SyncService* sync_service = nullptr;
  if (ProfileSyncServiceFactory::HasProfileSyncService(
          password_view_->GetProfile())) {
    sync_service =
        ProfileSyncServiceFactory::GetForProfile(password_view_->GetProfile());
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          *form, sync_service,
          SigninManagerFactory::GetForProfile(password_view_->GetProfile()))) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  // Call back the front end to reveal the password.
  password_view_->ShowPassword(index, form->password_value);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
#endif
}

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

bool PasswordManagerPresenter::TryRemovePasswordEntry(EntryKind entry_kind,
                                                      size_t index) {
  const auto& entries = entry_kind == EntryKind::kPassword
                            ? password_list_
                            : password_exception_list_;

  const auto* form = TryGetPasswordForm(entries, index);
  if (!form)
    return false;

  auto& duplicates = entry_kind == EntryKind::kPassword
                         ? password_duplicates_
                         : password_exception_duplicates_;

  PasswordStore* store = GetPasswordStore();
  if (!store)
    return false;

  RemoveDuplicates(*form, &duplicates, store);
  RemoveLogin(*form);
  return true;
}

void PasswordManagerPresenter::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  std::partition_copy(std::make_move_iterator(results.begin()),
                      std::make_move_iterator(results.end()),
                      std::back_inserter(password_exception_list_),
                      std::back_inserter(password_list_), [](const auto& form) {
                        return form->blacklisted_by_user;
                      });

  password_manager::SortEntriesAndHideDuplicates(&password_list_,
                                                 &password_duplicates_);
  password_manager::SortEntriesAndHideDuplicates(
      &password_exception_list_, &password_exception_duplicates_);

  SetPasswordList();
  SetPasswordExceptionList();
}

void PasswordManagerPresenter::SetPasswordList() {
  password_view_->SetPasswordList(password_list_);
}

void PasswordManagerPresenter::SetPasswordExceptionList() {
  password_view_->SetPasswordExceptionList(password_exception_list_);
}

PasswordStore* PasswordManagerPresenter::GetPasswordStore() {
  return PasswordStoreFactory::GetForProfile(password_view_->GetProfile(),
                                             ServiceAccessType::EXPLICIT_ACCESS)
      .get();
}
