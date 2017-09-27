// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
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
#include "components/browser_sync/profile_sync_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/sync/browser/password_sync_util.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/undo_operation.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#endif

using base::StringPiece;
using password_manager::PasswordStore;

namespace {

const char kSortKeyPartsSeparator = ' ';

// The character that is added to a sort key if there is no federation.
// Note: to separate the entries w/ federation and the entries w/o federation,
// this character should be alphabetically smaller than real federations.
const char kSortKeyNoFederationSymbol = '-';

// Creates key for sorting password or password exception entries. The key is
// eTLD+1 followed by the reversed list of domains (e.g.
// secure.accounts.example.com => example.com.com.example.accounts.secure) and
// the scheme. If |entry_type == SAVED|, username, password and federation are
// appended to the key. For Android credentials the canocial spec is included.
std::string CreateSortKey(const autofill::PasswordForm& form,
                          PasswordEntryType entry_type) {
  std::string shown_origin;
  GURL link_url;
  std::tie(shown_origin, link_url) =
      password_manager::GetShownOriginAndLinkUrl(form);

  const auto facet_uri =
      password_manager::FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  const bool is_android_uri = facet_uri.IsValidAndroidFacetURI();

  if (is_android_uri) {
    // In case of Android credentials |GetShownOriginAndLinkURl| might return
    // the app display name, e.g. the Play Store name of the given application.
    // This might or might not correspond to the eTLD+1, which is why
    // |shown_origin| is set to the reversed android package name in this case,
    // e.g. com.example.android => android.example.com.
    shown_origin = password_manager::SplitByDotAndReverse(
        facet_uri.android_package_name());
  }

  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          shown_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (site_name.empty())  // e.g. localhost.
    site_name = shown_origin;

  std::string key = site_name + kSortKeyPartsSeparator;

  // Since multiple distinct credentials might have the same site name, more
  // information is added. For Android credentials this includes the full
  // canonical spec which is guaranteed to be unique for a given App.
  key += is_android_uri ? facet_uri.canonical_spec()
                        : password_manager::SplitByDotAndReverse(shown_origin);

  if (entry_type == PasswordEntryType::SAVED) {
    key += kSortKeyPartsSeparator + base::UTF16ToUTF8(form.username_value) +
           kSortKeyPartsSeparator + base::UTF16ToUTF8(form.password_value);

    key += kSortKeyPartsSeparator;
    if (!form.federation_origin.unique())
      key += form.federation_origin.host();
    else
      key += kSortKeyNoFederationSymbol;
  }

  // To separate HTTP/HTTPS credentials, add the scheme to the key.
  return key += kSortKeyPartsSeparator + link_url.scheme();
}

// Finds duplicates of |form| in |duplicates|, removes them from |store| and
// from |duplicates|.
void RemoveDuplicates(const autofill::PasswordForm& form,
                      DuplicatesMap* duplicates,
                      PasswordStore* store,
                      PasswordEntryType entry_type) {
  std::string key = CreateSortKey(form, entry_type);
  std::pair<DuplicatesMap::iterator, DuplicatesMap::iterator> dups =
      duplicates->equal_range(key);
  for (DuplicatesMap::iterator it = dups.first; it != dups.second; ++it)
    store->RemoveLogin(*it->second);
  duplicates->erase(key);
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
    : populater_(this),
      exception_populater_(this),
      password_view_(password_view),
      password_manager_porter_(this) {
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
  password_duplicates_.clear();
  password_exception_list_.clear();
  password_exception_duplicates_.clear();

  populater_.Populate();
  exception_populater_.Populate();
}

void PasswordManagerPresenter::RemoveSavedPassword(size_t index) {
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer
    // (http://crbug.com/362054), or the user removed a password while a request
    // to the store is in progress (i.e. |password_list_| is empty).
    // Don't let it crash the browser.
    return;
  }
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  const autofill::PasswordForm& password_entry = *password_list_[index];
  RemoveDuplicates(password_entry, &password_duplicates_, store,
                   PasswordEntryType::SAVED);
  RemoveLogin(password_entry);
  base::RecordAction(
      base::UserMetricsAction("PasswordManager_RemoveSavedPassword"));
}

void PasswordManagerPresenter::RemovePasswordException(size_t index) {
  if (index >= password_exception_list_.size()) {
    // |index| out of bounds might come from a compromised renderer
    // (http://crbug.com/362054), or the user removed a password exception while
    // a request to the store is in progress (i.e. |password_exception_list_|
    // is empty). Don't let it crash the browser.
    return;
  }
  PasswordStore* store = GetPasswordStore();
  if (!store)
    return;

  const autofill::PasswordForm& password_exception_entry =
      *password_exception_list_[index];
  RemoveDuplicates(password_exception_entry, &password_exception_duplicates_,
                   store, PasswordEntryType::BLACKLISTED);
  RemoveLogin(password_exception_entry);
  base::RecordAction(
      base::UserMetricsAction("PasswordManager_RemovePasswordException"));
}

void PasswordManagerPresenter::UndoRemoveSavedPasswordOrException() {
  undo_manager_.Undo();
}

void PasswordManagerPresenter::RequestShowPassword(size_t index) {
#if !defined(OS_ANDROID)  // This is never called on Android.
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer
    // (http://crbug.com/362054), or the user requested to show a password while
    // a request to the store is in progress (i.e. |password_list_|
    // is empty). Don't let it crash the browser.
    return;
  }

  if (!IsUserAuthenticated()) {
    return;
  }

  syncer::SyncService* sync_service = nullptr;
  if (ProfileSyncServiceFactory::HasProfileSyncService(
          password_view_->GetProfile())) {
    sync_service =
        ProfileSyncServiceFactory::GetForProfile(password_view_->GetProfile());
  }
  if (password_manager::sync_util::IsSyncAccountCredential(
          *password_list_[index], sync_service,
          SigninManagerFactory::GetForProfile(password_view_->GetProfile()))) {
    base::RecordAction(
        base::UserMetricsAction("PasswordManager_SyncCredentialShown"));
  }

  // Call back the front end to reveal the password.
  std::string origin_url =
      extensions::CreateUrlCollectionFromForm(*password_list_[index]).origin;
  password_view_->ShowPassword(index, password_list_[index]->password_value);
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.AccessPasswordInSettings",
      password_manager::metrics_util::ACCESS_PASSWORD_VIEWED,
      password_manager::metrics_util::ACCESS_PASSWORD_COUNT);
#endif
}

std::vector<std::unique_ptr<autofill::PasswordForm>>
PasswordManagerPresenter::GetAllPasswords() {
  std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;

  for (const auto& form : password_list_) {
    ret_val.push_back(base::MakeUnique<autofill::PasswordForm>(*form));
  }

  return ret_val;
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPassword(
    size_t index) {
  if (index >= password_list_.size()) {
    // |index| out of bounds might come from a compromised renderer
    // (http://crbug.com/362054), or the user requested to get a password while
    // a request to the store is in progress (i.e. |password_list_|
    // is empty). Don't let it crash the browser.
    return NULL;
  }
  return password_list_[index].get();
}

const autofill::PasswordForm* PasswordManagerPresenter::GetPasswordException(
    size_t index) {
  if (index >= password_exception_list_.size()) {
    // |index| out of bounds might come from a compromised renderer
    // (http://crbug.com/362054), or the user requested to get a password
    // exception while a request to the store is in progress (i.e.
    // |password_exception_list_| is empty). Don't let it crash the browser.
    return NULL;
  }
  return password_exception_list_[index].get();
}

void PasswordManagerPresenter::SetPasswordList() {
  password_view_->SetPasswordList(password_list_);
}

void PasswordManagerPresenter::SetPasswordExceptionList() {
  password_view_->SetPasswordExceptionList(password_exception_list_);
}

void PasswordManagerPresenter::SortEntriesAndHideDuplicates(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* list,
    DuplicatesMap* duplicates,
    PasswordEntryType entry_type) {
  std::vector<std::pair<std::string, std::unique_ptr<autofill::PasswordForm>>>
      pairs;
  pairs.reserve(list->size());
  for (auto& form : *list) {
    pairs.push_back(
        std::make_pair(CreateSortKey(*form, entry_type), std::move(form)));
  }

  std::sort(
      pairs.begin(), pairs.end(),
      [](const std::pair<std::string, std::unique_ptr<autofill::PasswordForm>>&
             left,
         const std::pair<std::string, std::unique_ptr<autofill::PasswordForm>>&
             right) { return left.first < right.first; });

  list->clear();
  duplicates->clear();
  std::string previous_key;
  for (auto& pair : pairs) {
    if (pair.first != previous_key) {
      list->push_back(std::move(pair.second));
      previous_key = pair.first;
    } else {
      duplicates->insert(std::make_pair(previous_key, std::move(pair.second)));
    }
  }
}

bool PasswordManagerPresenter::IsUserAuthenticated() {
#if defined(OS_ANDROID)
  NOTREACHED();
#endif
  if (base::TimeTicks::Now() - last_authentication_time_ >
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
    UMA_HISTOGRAM_ENUMERATION(
        "PasswordManager.ReauthToAccessPasswordInSettings",
        authenticated ? password_manager::metrics_util::REAUTH_SUCCESS
                      : password_manager::metrics_util::REAUTH_FAILURE,
        password_manager::metrics_util::REAUTH_COUNT);
    return authenticated;
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordManager.ReauthToAccessPasswordInSettings",
                            password_manager::metrics_util::REAUTH_SKIPPED,
                            password_manager::metrics_util::REAUTH_COUNT);
  return true;
}

void PasswordManagerPresenter::ImportPasswords(
    content::WebContents* web_contents) {
  password_manager_porter_.PresentFileSelector(
      web_contents, PasswordManagerPorter::Type::PASSWORD_IMPORT);
}

void PasswordManagerPresenter::ExportPasswords(
    content::WebContents* web_contents) {
  password_manager_porter_.PresentFileSelector(
      web_contents, PasswordManagerPorter::Type::PASSWORD_EXPORT);
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
    CancelAllRequests();
    store->GetAutofillableLoginsWithAffiliationAndBrandingInformation(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display passwords.";
  }
}

void PasswordManagerPresenter::PasswordListPopulater::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  page_->password_list_ = std::move(results);
  page_->SortEntriesAndHideDuplicates(&page_->password_list_,
                                      &page_->password_duplicates_,
                                      PasswordEntryType::SAVED);
  page_->SetPasswordList();
}

PasswordManagerPresenter::PasswordExceptionListPopulater::
    PasswordExceptionListPopulater(PasswordManagerPresenter* page)
        : ListPopulater(page) {
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::Populate() {
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL) {
    CancelAllRequests();
    store->GetBlacklistLoginsWithAffiliationAndBrandingInformation(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display exceptions.";
  }
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::
    OnGetPasswordStoreResults(
        std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  page_->password_exception_list_ = std::move(results);
  page_->SortEntriesAndHideDuplicates(&page_->password_exception_list_,
                                      &page_->password_exception_duplicates_,
                                      PasswordEntryType::BLACKLISTED);
  page_->SetPasswordExceptionList();
}
