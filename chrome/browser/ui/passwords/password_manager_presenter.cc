// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_manager_presenter.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
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
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/import/password_importer.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/sync/browser/password_sync_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

#if defined(OS_WIN)
#include "chrome/browser/password_manager/password_manager_util_win.h"
#elif defined(OS_MACOSX)
#include "chrome/browser/password_manager/password_manager_util_mac.h"
#endif

using base::StringPiece;
using password_manager::PasswordStore;

namespace {

const int kAndroidAppSchemeAndDelimiterLength = 10;  // Length of 'android://'.

const char kSortKeyPartsSeparator = ' ';

// Reverse order of subdomains in hostname.
std::string SplitByDotAndReverse(StringPiece host) {
  std::vector<std::string> parts =
      base::SplitString(host, ".", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::reverse(parts.begin(), parts.end());
  return base::JoinString(parts, ".");
}

// Helper function that returns the type of the entry (non-Android credentials,
// Android w/ affiliated web realm (i.e. clickable) or w/o web realm).
std::string GetEntryTypeCode(bool is_android_uri, bool is_clickable) {
  if (!is_android_uri)
    return "0";
  if (is_clickable)
    return "1";
  return "2";
}

// Creates key for sorting password or password exception entries.
// The key is eTLD+1 followed by subdomains
// (e.g. secure.accounts.example.com => example.com.accounts.secure).
// If |username_and_password_in_key == true|, username and password is appended
// to the key. The entry type code (non-Android, Android w/ or w/o affiliated
// web realm) is also appended to the key.
std::string CreateSortKey(const autofill::PasswordForm& form,
                          bool username_and_password_in_key) {
  bool is_android_uri = false;
  bool is_clickable = false;
  GURL link_url;
  std::string origin = password_manager::GetShownOriginAndLinkUrl(
      form, &is_android_uri, &link_url, &is_clickable);

  if (!is_clickable) {  // e.g. android://com.example.r => r.example.com.
    origin = SplitByDotAndReverse(
        StringPiece(&origin[kAndroidAppSchemeAndDelimiterLength],
                    origin.length() - kAndroidAppSchemeAndDelimiterLength));
  }

  std::string site_name =
      net::registry_controlled_domains::GetDomainAndRegistry(
          origin, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (site_name.empty())  // e.g. localhost.
    site_name = origin;
  std::string key =
      site_name + SplitByDotAndReverse(StringPiece(
                      &origin[0], origin.length() - site_name.length()));

  if (username_and_password_in_key) {
    key = key + kSortKeyPartsSeparator +
          base::UTF16ToUTF8(form.username_value) + kSortKeyPartsSeparator +
          base::UTF16ToUTF8(form.password_value);
  }

  // Since Android and non-Android entries shouldn't be merged into one entry,
  // add the entry type code to the sort key.
  key +=
      kSortKeyPartsSeparator + GetEntryTypeCode(is_android_uri, is_clickable);
  return key;
}

// Finds duplicates of |form| in |duplicates|, removes them from |store| and
// from |duplicates|.
void RemoveDuplicates(const autofill::PasswordForm& form,
                      DuplicatesMap* duplicates,
                      PasswordStore* store,
                      bool username_and_password_in_key) {
  std::string key =
      CreateSortKey(form, username_and_password_in_key);
  std::pair<DuplicatesMap::iterator, DuplicatesMap::iterator> dups =
      duplicates->equal_range(key);
  for (DuplicatesMap::iterator it = dups.first; it != dups.second; ++it)
    store->RemoveLogin(*it->second);
  duplicates->erase(key);
}

}  // namespace

PasswordManagerPresenter::PasswordManagerPresenter(
    PasswordUIView* password_view)
    : populater_(this),
      exception_populater_(this),
      password_view_(password_view) {
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

  RemoveDuplicates(*password_list_[index], &password_duplicates_,
                   store, true);
  store->RemoveLogin(*password_list_[index]);
  content::RecordAction(
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
  RemoveDuplicates(*password_exception_list_[index],
                   &password_exception_duplicates_, store, false);
  store->RemoveLogin(*password_exception_list_[index]);
  content::RecordAction(
      base::UserMetricsAction("PasswordManager_RemovePasswordException"));
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
      *password_list_[index]);
  password_view_->ShowPassword(
      index,
      origin_url,
      base::UTF16ToUTF8(password_list_[index]->username_value),
      password_list_[index]->password_value);
#endif
}

std::vector<std::unique_ptr<autofill::PasswordForm>>
PasswordManagerPresenter::GetAllPasswords() {
  std::vector<std::unique_ptr<autofill::PasswordForm>> ret_val;

  for (const auto& form : password_list_) {
    ret_val.push_back(base::WrapUnique(new autofill::PasswordForm(*form)));
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
    bool username_and_password_in_key) {
  std::vector<std::pair<std::string, std::unique_ptr<autofill::PasswordForm>>>
      pairs;
  pairs.reserve(list->size());
  for (auto& form : *list) {
    pairs.push_back(std::make_pair(
        CreateSortKey(*form, username_and_password_in_key), std::move(form)));
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
    return authenticated;
  }
  return true;
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
    store->GetAutofillableLoginsWithAffiliatedRealms(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display passwords.";
  }
}

void PasswordManagerPresenter::PasswordListPopulater::OnGetPasswordStoreResults(
    ScopedVector<autofill::PasswordForm> results) {
  page_->password_list_ =
      password_manager_util::ConvertScopedVector(std::move(results));
  page_->SortEntriesAndHideDuplicates(&page_->password_list_,
                                      &page_->password_duplicates_,
                                      true /* use username and password */);
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
    store->GetBlacklistLoginsWithAffiliatedRealms(this);
  } else {
    LOG(ERROR) << "No password store! Cannot display exceptions.";
  }
}

void PasswordManagerPresenter::PasswordExceptionListPopulater::
    OnGetPasswordStoreResults(ScopedVector<autofill::PasswordForm> results) {
  page_->password_exception_list_ =
      password_manager_util::ConvertScopedVector(std::move(results));
  page_->SortEntriesAndHideDuplicates(&page_->password_exception_list_,
      &page_->password_exception_duplicates_,
      false /* don't use username and password*/);
  page_->SetPasswordExceptionList();
}
