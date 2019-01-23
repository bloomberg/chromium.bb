// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search_suggest/search_suggest_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/search/search_suggest/search_suggest_loader.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "services/identity/public/cpp/identity_manager.h"

class SearchSuggestService::SigninObserver
    : public identity::IdentityManager::Observer {
 public:
  using SigninStatusChangedCallback = base::RepeatingClosure;

  SigninObserver(identity::IdentityManager* identity_manager,
                 const SigninStatusChangedCallback& callback)
      : identity_manager_(identity_manager), callback_(callback) {
    identity_manager_->AddObserver(this);
  }

  ~SigninObserver() override { identity_manager_->RemoveObserver(this); }

  bool SignedIn() {
    return !identity_manager_->GetAccountsInCookieJar()
                .signed_in_accounts.empty();
  }

 private:
  // IdentityManager::Observer implementation.
  void OnAccountsInCookieUpdated(
      const identity::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override {
    callback_.Run();
  }

  identity::IdentityManager* const identity_manager_;
  SigninStatusChangedCallback callback_;
};

SearchSuggestService::SearchSuggestService(
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    std::unique_ptr<SearchSuggestLoader> loader)
    : loader_(std::move(loader)),
      signin_observer_(std::make_unique<SigninObserver>(
          identity_manager,
          base::BindRepeating(&SearchSuggestService::SigninStatusChanged,
                              base::Unretained(this)))),
      pref_service_(pref_service) {}

SearchSuggestService::~SearchSuggestService() = default;

void SearchSuggestService::Shutdown() {
  for (auto& observer : observers_) {
    observer.OnSearchSuggestServiceShuttingDown();
  }

  signin_observer_.reset();
  DCHECK(!observers_.might_have_observers());
}

void SearchSuggestService::Refresh() {
  if (!signin_observer_->SignedIn()) {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::SIGNED_OUT,
                            base::nullopt);
  } else if (pref_service_->GetBoolean(prefs::kNtpSearchSuggestionsOptOut)) {
    SearchSuggestDataLoaded(SearchSuggestLoader::Status::OPTED_OUT,
                            base::nullopt);
  } else {
    const std::string blacklist = GetBlacklistAsString();
    loader_->Load(blacklist,
                  base::BindOnce(&SearchSuggestService::SearchSuggestDataLoaded,
                                 base::Unretained(this)));
  }
}

void SearchSuggestService::AddObserver(SearchSuggestServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchSuggestService::RemoveObserver(
    SearchSuggestServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SearchSuggestService::SigninStatusChanged() {
  // If we have cached data, clear it.
  if (search_suggest_data_.has_value()) {
    search_suggest_data_ = base::nullopt;
  }
}

void SearchSuggestService::SearchSuggestDataLoaded(
    SearchSuggestLoader::Status status,
    const base::Optional<SearchSuggestData>& data) {
  // In case of transient errors, keep our cached data (if any), but still
  // notify observers of the finished load (attempt).
  if (status != SearchSuggestLoader::Status::TRANSIENT_ERROR) {
    // TODO(crbug/904565): Verify that cached data is also cleared when the
    // impression cap is reached. Including the response from the request made
    // on the same load that the cap was hit.
    search_suggest_data_ = data;
  }
  NotifyObservers();
}

void SearchSuggestService::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnSearchSuggestDataUpdated();
  }
}

void SearchSuggestService::BlacklistSearchSuggestion(int task_version,
                                                     long task_id) {
  std::string task_version_id =
      std::to_string(task_version) + "_" + std::to_string(task_id);
  DictionaryPrefUpdate update(pref_service_,
                              prefs::kNtpSearchSuggestionsBlacklist);
  base::DictionaryValue* blacklist = update.Get();
  blacklist->SetKey(task_version_id, base::ListValue());

  search_suggest_data_ = base::nullopt;
  Refresh();
}

void SearchSuggestService::BlacklistSearchSuggestionWithHash(
    int task_version,
    long task_id,
    const std::vector<uint8_t>& hash) {
  std::string task_version_id =
      std::to_string(task_version) + "_" + std::to_string(task_id);
  std::string hash_string;
  hash_string.assign(hash.begin(), hash.end());
  DictionaryPrefUpdate update(pref_service_,
                              prefs::kNtpSearchSuggestionsBlacklist);
  base::DictionaryValue* blacklist = update.Get();
  base::Value* value = blacklist->FindKey(task_version_id);
  if (!value)
    value = blacklist->SetKey(task_version_id, base::ListValue());
  value->GetList().emplace_back(base::Value(hash_string));

  search_suggest_data_ = base::nullopt;
  Refresh();
}

std::string SearchSuggestService::GetBlacklistAsString() {
  const base::DictionaryValue* blacklist =
      pref_service_->GetDictionary(prefs::kNtpSearchSuggestionsBlacklist);

  std::string blacklist_as_string;
  for (const auto& dict : blacklist->DictItems()) {
    blacklist_as_string += dict.first;

    if (!dict.second.GetList().empty()) {
      std::string list = ":";

      for (const auto& i : dict.second.GetList()) {
        list += i.GetString() + ",";
      }

      // Remove trailing comma.
      list.pop_back();
      blacklist_as_string += list;
    }

    blacklist_as_string += ";";
  }

  // Remove trailing semi-colon.
  if (!blacklist_as_string.empty())
    blacklist_as_string.pop_back();
  return blacklist_as_string;
}

void SearchSuggestService::OptOutOfSearchSuggestions() {
  pref_service_->SetBoolean(prefs::kNtpSearchSuggestionsOptOut, true);

  search_suggest_data_ = base::nullopt;
}

// static
void SearchSuggestService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpSearchSuggestionsBlacklist);
  registry->RegisterBooleanPref(prefs::kNtpSearchSuggestionsOptOut, false);
}
