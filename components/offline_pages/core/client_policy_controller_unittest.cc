// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_policy_controller.h"

#include <algorithm>
#include <memory>
#include <set>

#include "base/stl_util.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {
const char kUndefinedNamespace[] = "undefined";

bool isTemporary(const OfflinePageClientPolicy& policy) {
  return policy.lifetime_type == LifetimeType::TEMPORARY;
}
}  // namespace

class ClientPolicyControllerTest : public testing::Test {
 public:
  ClientPolicyController* controller() { return controller_.get(); }

  // testing::Test
  void SetUp() override;
  void TearDown() override;

 protected:
  void ExpectTemporary(std::string name_space);
  void ExpectDownloadSupport(std::string name_space, bool expectation);
  void ExpectPersistent(std::string name_space);
  void ExpectRestrictedToTabFromClientId(std::string name_space,
                                         bool expectation);
  void ExpectRequiresSpecificUserSettings(std::string name_space,
                                          bool expectation);

 private:
  std::unique_ptr<ClientPolicyController> controller_;
};

void ClientPolicyControllerTest::SetUp() {
  controller_.reset(new ClientPolicyController());
}

void ClientPolicyControllerTest::TearDown() {
  controller_.reset();
}

void ClientPolicyControllerTest::ExpectTemporary(std::string name_space) {
  EXPECT_TRUE(
      base::Contains(controller()->GetTemporaryNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting temporary namespaces.";
  EXPECT_TRUE(controller()->IsTemporary(name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type setting when directly checking"
         " if it is temporary.";
  EXPECT_FALSE(
      base::Contains(controller()->GetPersistentNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting persistent namespaces.";
  EXPECT_FALSE(controller()->IsPersistent(name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type setting when directly checking"
         " if it is persistent.";
}

void ClientPolicyControllerTest::ExpectDownloadSupport(std::string name_space,
                                                       bool expectation) {
  EXPECT_EQ(expectation, controller()->IsSupportedByDownload(name_space))
      << "Namespace " << name_space
      << " had incorrect download support when directly checking if supported"
         " by download.";
}

void ClientPolicyControllerTest::ExpectPersistent(std::string name_space) {
  EXPECT_FALSE(
      base::Contains(controller()->GetTemporaryNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting temporary namespaces.";
  EXPECT_FALSE(controller()->IsTemporary(name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type setting when directly checking"
         " if it is temporary.";
  EXPECT_TRUE(
      base::Contains(controller()->GetPersistentNamespaces(), name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type when getting persistent namespaces.";
  EXPECT_TRUE(controller()->IsPersistent(name_space))
      << "Namespace " << name_space
      << " had incorrect lifetime type setting when directly checking"
         " if it is persistent.";
}

void ClientPolicyControllerTest::ExpectRestrictedToTabFromClientId(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(expectation,
            controller()->IsRestrictedToTabFromClientId(name_space))
      << "Namespace " << name_space
      << " had incorrect restriction when directly checking if the namespace"
         " is restricted to the tab from the client id field";
}

void ClientPolicyControllerTest::ExpectRequiresSpecificUserSettings(
    std::string name_space,
    bool expectation) {
  EXPECT_EQ(expectation, controller()->RequiresSpecificUserSettings(name_space))
      << "Namespace " << name_space
      << " had incorrect download support when directly checking if disabled"
         " when prefetch settings are disabled.";
}

TEST_F(ClientPolicyControllerTest, FallbackTest) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kUndefinedNamespace);
  EXPECT_EQ(policy.name_space, kDefaultNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kDefaultNamespace);
  EXPECT_FALSE(base::Contains(controller()->GetTemporaryNamespaces(),
                              kUndefinedNamespace));
  EXPECT_TRUE(controller()->IsTemporary(kUndefinedNamespace));
  ExpectDownloadSupport(kUndefinedNamespace, false);
  ExpectDownloadSupport(kDefaultNamespace, false);
  ExpectRestrictedToTabFromClientId(kUndefinedNamespace, false);
  ExpectRestrictedToTabFromClientId(kDefaultNamespace, false);
  ExpectRequiresSpecificUserSettings(kUndefinedNamespace, false);
  ExpectRequiresSpecificUserSettings(kDefaultNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckBookmarkDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kBookmarkNamespace);
  EXPECT_EQ(policy.name_space, kBookmarkNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kBookmarkNamespace);
  ExpectDownloadSupport(kBookmarkNamespace, false);
  ExpectRestrictedToTabFromClientId(kBookmarkNamespace, false);
  ExpectRequiresSpecificUserSettings(kBookmarkNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckLastNDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kLastNNamespace);
  EXPECT_EQ(policy.name_space, kLastNNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kLastNNamespace);
  ExpectDownloadSupport(kLastNNamespace, false);
  ExpectRestrictedToTabFromClientId(kLastNNamespace, true);
  ExpectRequiresSpecificUserSettings(kLastNNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckAsyncDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kAsyncNamespace);
  EXPECT_EQ(policy.name_space, kAsyncNamespace);
  EXPECT_FALSE(isTemporary(policy));
  ExpectDownloadSupport(kAsyncNamespace, true);
  ExpectPersistent(kAsyncNamespace);
  ExpectRestrictedToTabFromClientId(kAsyncNamespace, false);
  ExpectRequiresSpecificUserSettings(kAsyncNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckCCTDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kCCTNamespace);
  EXPECT_EQ(policy.name_space, kCCTNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kCCTNamespace);
  ExpectDownloadSupport(kCCTNamespace, false);
  ExpectRestrictedToTabFromClientId(kCCTNamespace, false);
  ExpectRequiresSpecificUserSettings(kCCTNamespace, true);
}

TEST_F(ClientPolicyControllerTest, CheckDownloadDefined) {
  OfflinePageClientPolicy policy = controller()->GetPolicy(kDownloadNamespace);
  EXPECT_EQ(policy.name_space, kDownloadNamespace);
  EXPECT_FALSE(isTemporary(policy));
  ExpectDownloadSupport(kDownloadNamespace, true);
  ExpectPersistent(kDownloadNamespace);
  ExpectRestrictedToTabFromClientId(kDownloadNamespace, false);
  ExpectRequiresSpecificUserSettings(kDownloadNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckNTPSuggestionsDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kNTPSuggestionsNamespace);
  EXPECT_EQ(policy.name_space, kNTPSuggestionsNamespace);
  EXPECT_FALSE(isTemporary(policy));
  ExpectDownloadSupport(kNTPSuggestionsNamespace, true);
  ExpectPersistent(kNTPSuggestionsNamespace);
  ExpectRestrictedToTabFromClientId(kNTPSuggestionsNamespace, false);
  ExpectRequiresSpecificUserSettings(kNTPSuggestionsNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckSuggestedArticlesDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kSuggestedArticlesNamespace);
  EXPECT_EQ(policy.name_space, kSuggestedArticlesNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kSuggestedArticlesNamespace);
  ExpectDownloadSupport(kSuggestedArticlesNamespace, IsOfflinePagesEnabled());
  ExpectRestrictedToTabFromClientId(kSuggestedArticlesNamespace, false);
  ExpectRequiresSpecificUserSettings(kSuggestedArticlesNamespace, false);
}

TEST_F(ClientPolicyControllerTest, CheckLivePageSharingDefined) {
  OfflinePageClientPolicy policy =
      controller()->GetPolicy(kLivePageSharingNamespace);
  EXPECT_EQ(policy.name_space, kLivePageSharingNamespace);
  EXPECT_TRUE(isTemporary(policy));
  ExpectTemporary(kLivePageSharingNamespace);
  ExpectDownloadSupport(kLivePageSharingNamespace, false);
  ExpectRestrictedToTabFromClientId(kLivePageSharingNamespace, true);
  ExpectRequiresSpecificUserSettings(kLivePageSharingNamespace, false);
}

TEST_F(ClientPolicyControllerTest, AllTemporaryNamespaces) {
  std::vector<std::string> all_namespaces = controller()->GetAllNamespaces();
  const std::vector<std::string>& cache_reset_namespaces_list =
      controller()->GetTemporaryNamespaces();
  std::set<std::string> cache_reset_namespaces(
      cache_reset_namespaces_list.begin(), cache_reset_namespaces_list.end());
  for (auto name_space : cache_reset_namespaces) {
    if (cache_reset_namespaces.count(name_space) > 0)
      ExpectTemporary(name_space);
    else
      ExpectPersistent(name_space);
  }
}

}  // namespace offline_pages
