// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/remote/test_utils.h"

#include <limits>
#include <memory>

#include "base/memory/ptr_util.h"
#include "components/ntp_snippets/remote/proto/ntp_snippets.pb.h"
#include "components/ntp_snippets/time_serialization.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/sync/driver/fake_sync_service.h"

namespace ntp_snippets {

namespace test {

namespace {

base::Time GetDefaultSuggestionCreationTime() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCString("2000-01-01T00:00:01Z", &out_time));
  return out_time;
}

base::Time GetDefaultSuggestionExpirationTime() {
  base::Time out_time;
  EXPECT_TRUE(base::Time::FromUTCString("2100-01-01T00:00:01Z", &out_time));
  return out_time;
}

}  // namespace

FakeSyncService::FakeSyncService()
    : can_sync_start_(true),
      is_sync_active_(true),
      configuration_done_(true),
      is_encrypt_everything_enabled_(false),
      active_data_types_(syncer::HISTORY_DELETE_DIRECTIVES) {}

FakeSyncService::~FakeSyncService() = default;

bool FakeSyncService::CanSyncStart() const {
  return can_sync_start_;
}

bool FakeSyncService::IsSyncActive() const {
  return is_sync_active_;
}

bool FakeSyncService::ConfigurationDone() const {
  return configuration_done_;
}

bool FakeSyncService::IsEncryptEverythingEnabled() const {
  return is_encrypt_everything_enabled_;
}

syncer::ModelTypeSet FakeSyncService::GetActiveDataTypes() const {
  return active_data_types_;
}

RemoteSuggestionsTestUtils::RemoteSuggestionsTestUtils()
    : pref_service_(base::MakeUnique<TestingPrefServiceSyncable>()) {
  AccountTrackerService::RegisterPrefs(pref_service_->registry());

#if defined(OS_CHROMEOS)
  SigninManagerBase::RegisterProfilePrefs(pref_service_->registry());
  SigninManagerBase::RegisterPrefs(pref_service_->registry());
#else
  SigninManager::RegisterProfilePrefs(pref_service_->registry());
  SigninManager::RegisterPrefs(pref_service_->registry());
#endif  // OS_CHROMEOS

  token_service_ = base::MakeUnique<FakeProfileOAuth2TokenService>();
  signin_client_ = base::MakeUnique<TestSigninClient>(pref_service_.get());
  account_tracker_ = base::MakeUnique<AccountTrackerService>();
  account_tracker_->Initialize(signin_client_.get());
  fake_sync_service_ = base::MakeUnique<FakeSyncService>();

  ResetSigninManager();
}

RemoteSuggestionsTestUtils::~RemoteSuggestionsTestUtils() = default;

void RemoteSuggestionsTestUtils::ResetSigninManager() {
#if defined(OS_CHROMEOS)
  fake_signin_manager_ = base::MakeUnique<FakeSigninManagerBase>(
      signin_client_.get(), account_tracker_.get());
#else
  fake_signin_manager_ = base::MakeUnique<FakeSigninManager>(
      signin_client_.get(), token_service_.get(), account_tracker_.get(),
      /*cookie_manager_service=*/nullptr);
#endif
}

RemoteSuggestionBuilder::RemoteSuggestionBuilder() = default;
RemoteSuggestionBuilder::RemoteSuggestionBuilder(
    const RemoteSuggestionBuilder& other) = default;
RemoteSuggestionBuilder::~RemoteSuggestionBuilder() = default;

RemoteSuggestionBuilder& RemoteSuggestionBuilder::AddId(const std::string& id) {
  if (!ids_) {
    ids_ = std::vector<std::string>();
  }
  ids_->push_back(id);
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetTitle(
    const std::string& title) {
  title_ = title;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetSnippet(
    const std::string& snippet) {
  snippet_ = snippet;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetImageUrl(
    const std::string& image_url) {
  salient_image_url_ = image_url;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetPublishDate(
    const base::Time& publish_date) {
  publish_date_ = publish_date;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetExpiryDate(
    const base::Time& expiry_date) {
  expiry_date_ = expiry_date;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetScore(double score) {
  score_ = score;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetIsDismissed(
    bool is_dismissed) {
  is_dismissed_ = is_dismissed;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetRemoteCategoryId(
    int remote_category_id) {
  remote_category_id_ = remote_category_id;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetUrl(
    const std::string& url) {
  url_ = url;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetPublisher(
    const std::string& publisher) {
  publisher_name_ = publisher;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetAmpUrl(
    const std::string& amp_url) {
  amp_url_ = amp_url;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetFetchDate(
    const base::Time& fetch_date) {
  fetch_date_ = fetch_date;
  return *this;
}

RemoteSuggestionBuilder& RemoteSuggestionBuilder::SetRank(int rank) {
  rank_ = rank;
  return *this;
}

std::unique_ptr<RemoteSuggestion> RemoteSuggestionBuilder::Build() const {
  SnippetProto proto;
  proto.set_title(title_.value_or("Title"));
  proto.set_snippet(snippet_.value_or("Snippet"));
  proto.set_salient_image_url(
      salient_image_url_.value_or("http://image_url.com/"));
  proto.set_publish_date(SerializeTime(
      publish_date_.value_or(GetDefaultSuggestionCreationTime())));
  proto.set_expiry_date(SerializeTime(
      expiry_date_.value_or(GetDefaultSuggestionExpirationTime())));
  proto.set_score(score_.value_or(1));
  proto.set_dismissed(is_dismissed_.value_or(false));
  proto.set_remote_category_id(remote_category_id_.value_or(1));
  auto* source = proto.add_sources();
  source->set_url(url_.value_or("http://url.com/"));
  source->set_publisher_name(publisher_name_.value_or("Publisher"));
  source->set_amp_url(amp_url_.value_or("http://amp_url.com/"));
  proto.set_fetch_date(SerializeTime(fetch_date_.value_or(base::Time::Now())));
  for (const auto& id :
       ids_.value_or(std::vector<std::string>{source->url()})) {
    proto.add_ids(id);
  }
  proto.set_rank(rank_.value_or(std::numeric_limits<int>::max()));
  return RemoteSuggestion::CreateFromProto(proto);
}

FetchedCategoryBuilder::FetchedCategoryBuilder() = default;
FetchedCategoryBuilder::FetchedCategoryBuilder(
    const FetchedCategoryBuilder& other) = default;
FetchedCategoryBuilder::~FetchedCategoryBuilder() = default;

FetchedCategoryBuilder& FetchedCategoryBuilder::SetCategory(Category category) {
  category_ = category;
  return *this;
}

FetchedCategoryBuilder& FetchedCategoryBuilder::SetTitle(
    const std::string& title) {
  title_ = base::UTF8ToUTF16(title);
  return *this;
}

FetchedCategoryBuilder& FetchedCategoryBuilder::SetCardLayout(
    ContentSuggestionsCardLayout card_layout) {
  card_layout_ = card_layout;
  return *this;
}

FetchedCategoryBuilder& FetchedCategoryBuilder::SetAdditionalAction(
    ContentSuggestionsAdditionalAction additional_action) {
  additional_action_ = additional_action;
  return *this;
}

FetchedCategoryBuilder& FetchedCategoryBuilder::SetShowIfEmpty(
    bool show_if_empty) {
  show_if_empty_ = show_if_empty;
  return *this;
}

FetchedCategoryBuilder& FetchedCategoryBuilder::SetNoSuggestionsMessage(
    const std::string& no_suggestions_message) {
  no_suggestions_message_ = base::UTF8ToUTF16(no_suggestions_message);
  return *this;
}

FetchedCategoryBuilder& FetchedCategoryBuilder::AddSuggestionViaBuilder(
    const RemoteSuggestionBuilder& builder) {
  if (!suggestion_builders_) {
    suggestion_builders_ = std::vector<RemoteSuggestionBuilder>();
  }
  suggestion_builders_->push_back(builder);
  return *this;
}

FetchedCategory FetchedCategoryBuilder::Build() const {
  FetchedCategory result = FetchedCategory(
      category_.value_or(Category::FromRemoteCategory(1)),
      CategoryInfo(
          title_.value_or(base::UTF8ToUTF16("Category title")),
          card_layout_.value_or(ContentSuggestionsCardLayout::FULL_CARD),
          additional_action_.value_or(
              ContentSuggestionsAdditionalAction::FETCH),
          show_if_empty_.value_or(false),
          no_suggestions_message_.value_or(
              base::UTF8ToUTF16("No suggestions message"))));

  if (suggestion_builders_) {
    for (const auto& suggestion_builder : *suggestion_builders_)
      result.suggestions.push_back(suggestion_builder.Build());
  }
  return result;
}

}  // namespace test

}  // namespace ntp_snippets
