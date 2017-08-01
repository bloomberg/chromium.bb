// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_TEST_UTILS_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "build/build_config.h"
#include "components/ntp_snippets/remote/json_to_categories.h"
#include "components/ntp_snippets/remote/remote_suggestion.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class AccountTrackerService;
class FakeProfileOAuth2TokenService;
class TestSigninClient;

using sync_preferences::TestingPrefServiceSyncable;

#if defined(OS_CHROMEOS)
// ChromeOS doesn't have SigninManager.
using SigninManagerForTest = FakeSigninManagerBase;
#else
using SigninManagerForTest = FakeSigninManager;
#endif  // OS_CHROMEOS

namespace ntp_snippets {

namespace test {

class FakeSyncService : public syncer::FakeSyncService {
 public:
  FakeSyncService();
  ~FakeSyncService() override;

  bool CanSyncStart() const override;
  bool IsSyncActive() const override;
  bool ConfigurationDone() const override;
  bool IsEncryptEverythingEnabled() const override;
  syncer::ModelTypeSet GetActiveDataTypes() const override;

  bool can_sync_start_;
  bool is_sync_active_;
  bool configuration_done_;
  bool is_encrypt_everything_enabled_;
  syncer::ModelTypeSet active_data_types_;
};

// Common utilities for remote suggestion tests, handles initializing fakes for
// sync and signin.
class RemoteSuggestionsTestUtils {
 public:
  RemoteSuggestionsTestUtils();
  ~RemoteSuggestionsTestUtils();

  void ResetSigninManager();

  FakeSyncService* fake_sync_service() { return fake_sync_service_.get(); }
  SigninManagerForTest* fake_signin_manager() {
    return fake_signin_manager_.get();
  }
  TestingPrefServiceSyncable* pref_service() { return pref_service_.get(); }
  FakeProfileOAuth2TokenService* token_service() {
    return token_service_.get();
  }

 private:
  std::unique_ptr<SigninManagerForTest> fake_signin_manager_;
  std::unique_ptr<FakeSyncService> fake_sync_service_;
  std::unique_ptr<TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<FakeProfileOAuth2TokenService> token_service_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
};

class RemoteSuggestionBuilder {
 public:
  RemoteSuggestionBuilder();
  RemoteSuggestionBuilder(const RemoteSuggestionBuilder& other);
  ~RemoteSuggestionBuilder();

  RemoteSuggestionBuilder& AddId(const std::string& id);
  RemoteSuggestionBuilder& SetTitle(const std::string& title);
  RemoteSuggestionBuilder& SetSnippet(const std::string& snippet);
  RemoteSuggestionBuilder& SetImageUrl(const std::string& image_url);
  RemoteSuggestionBuilder& SetPublishDate(const base::Time& publish_date);
  RemoteSuggestionBuilder& SetExpiryDate(const base::Time& expiry_date);
  RemoteSuggestionBuilder& SetScore(double score);
  RemoteSuggestionBuilder& SetIsDismissed(bool is_dismissed);
  RemoteSuggestionBuilder& SetRemoteCategoryId(int remote_category_id);
  RemoteSuggestionBuilder& SetUrl(const std::string& url);
  RemoteSuggestionBuilder& SetPublisher(const std::string& publisher);
  RemoteSuggestionBuilder& SetAmpUrl(const std::string& amp_url);
  RemoteSuggestionBuilder& SetFetchDate(const base::Time& fetch_date);
  RemoteSuggestionBuilder& SetRank(int rank);

  std::unique_ptr<RemoteSuggestion> Build() const;

 private:
  base::Optional<std::vector<std::string>> ids_;
  base::Optional<std::string> title_;
  base::Optional<std::string> snippet_;
  base::Optional<std::string> salient_image_url_;
  base::Optional<base::Time> publish_date_;
  base::Optional<base::Time> expiry_date_;
  base::Optional<double> score_;
  base::Optional<bool> is_dismissed_;
  base::Optional<int> remote_category_id_;
  base::Optional<std::string> url_;
  base::Optional<std::string> publisher_name_;
  base::Optional<std::string> amp_url_;
  base::Optional<base::Time> fetch_date_;
  base::Optional<int> rank_;
};

class FetchedCategoryBuilder {
 public:
  FetchedCategoryBuilder();
  FetchedCategoryBuilder(const FetchedCategoryBuilder& other);
  ~FetchedCategoryBuilder();

  FetchedCategoryBuilder& SetCategory(Category category);
  FetchedCategoryBuilder& SetTitle(const std::string& title);
  FetchedCategoryBuilder& SetCardLayout(
      ContentSuggestionsCardLayout card_layout);
  FetchedCategoryBuilder& SetAdditionalAction(
      ContentSuggestionsAdditionalAction additional_action);
  FetchedCategoryBuilder& SetShowIfEmpty(bool show_if_empty);
  FetchedCategoryBuilder& SetNoSuggestionsMessage(
      const std::string& no_suggestions_message);
  FetchedCategoryBuilder& AddSuggestionViaBuilder(
      const RemoteSuggestionBuilder& builder);

  FetchedCategory Build() const;

 private:
  base::Optional<Category> category_;
  base::Optional<base::string16> title_;
  base::Optional<ContentSuggestionsCardLayout> card_layout_;
  base::Optional<ContentSuggestionsAdditionalAction> additional_action_;
  base::Optional<bool> show_if_empty_;
  base::Optional<base::string16> no_suggestions_message_;
  base::Optional<std::vector<RemoteSuggestionBuilder>> suggestion_builders_;
};

}  // namespace test

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_TEST_UTILS_H_
