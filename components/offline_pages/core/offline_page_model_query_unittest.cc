// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_model_query.h"

#include "base/time/time.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

using Requirement = OfflinePageModelQueryBuilder::Requirement;
using URLSearchParams = OfflinePageModelQuery::URLSearchParams;

namespace {

const ClientId kClientId1 = {kDefaultNamespace, "id1"};
const GURL kUrl1 = GURL("https://ktestitem1.com");
const OfflinePageItem kTestItem1(kUrl1, 1, kClientId1, base::FilePath(), 1);

const ClientId kClientId2 = {kDefaultNamespace, "id2"};
const GURL kUrl2 = GURL("https://ktestitem2.com");
const OfflinePageItem kTestItem2(kUrl2, 2, kClientId2, base::FilePath(), 2);

const char kTestNamespace[] = "test_namespace";
const GURL kTempUrl = GURL("https://temp.temp");
const GURL kTempFragUrl = GURL("https://temp.temp#frag1");
const GURL kFragUrl1 = GURL("https://ktestitem1.com#frag");
}  // namespace

class OfflinePageModelQueryTest : public testing::Test {
 public:
  OfflinePageModelQueryTest();
  ~OfflinePageModelQueryTest() override;

 protected:
  ClientPolicyController policy_;
  OfflinePageModelQueryBuilder builder_;

  const OfflinePageItem cache_page() {
    return OfflinePageItem(GURL("https://download.com"), 3,
                           {kBookmarkNamespace, "id1"}, base::FilePath(), 3);
  }
  const OfflinePageItem download_page() {
    return OfflinePageItem(GURL("https://download.com"), 4,
                           {kDownloadNamespace, "id1"}, base::FilePath(), 4);
  }

  const OfflinePageItem original_tab_page() {
    return OfflinePageItem(GURL("https://download.com"), 5,
                           {kLastNNamespace, "id1"}, base::FilePath(), 5);
  }

  const OfflinePageItem test_namespace_page() {
    return OfflinePageItem(GURL("https://download.com"), 6,
                           {kTestNamespace, "id1"}, base::FilePath(), 6);
  }

  const OfflinePageItem recent_page() {
    return OfflinePageItem(GURL("https://download.com"), 7,
                           {kLastNNamespace, "id1"}, base::FilePath(), 7);
  }

  const OfflinePageItem cct_page() {
    OfflinePageItem page = kTestItem1;
    page.request_origin = "[\"abc.xyz\",[\"12345\"]]";
    return page;
  }
  const OfflinePageItem CreatePageWithUrls(const GURL& url,
                                           const GURL& original_url) {
    OfflinePageItem page = kTestItem1;
    page.url = url;
    page.original_url = original_url;
    return page;
  }
};

OfflinePageModelQueryTest::OfflinePageModelQueryTest() {
  policy_.AddPolicyForTest(
      kTestNamespace,
      OfflinePageClientPolicyBuilder(kTestNamespace,
                                     LifetimePolicy::LifetimeType::TEMPORARY,
                                     kUnlimitedPages, kUnlimitedPages)
          .SetIsOnlyShownInOriginalTab(true));
}

OfflinePageModelQueryTest::~OfflinePageModelQueryTest() {}

TEST_F(OfflinePageModelQueryTest, DefaultValues) {
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  EXPECT_NE(nullptr, query.get());
  EXPECT_EQ(Requirement::UNSET, query->GetRestrictedToOfflineIds().first);
  EXPECT_FALSE(query->GetRestrictedToNamespaces().first);

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, OfflinePageIdsSet_Exclude) {
  std::vector<int64_t> ids = {1, 4, 9, 16};
  builder_.SetOfflinePageIds(Requirement::EXCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::set<int64_t>> offline_id_restriction =
      query->GetRestrictedToOfflineIds();
  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, offline_id_restriction.first);

  ASSERT_EQ(ids.size(), offline_id_restriction.second.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, offline_id_restriction.second.count(id))
        << "Did not find " << id << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, OfflinePageIdsSet) {
  std::vector<int64_t> ids = {1, 4, 9, 16};
  builder_.SetOfflinePageIds(Requirement::INCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::set<int64_t>> offline_id_restriction =
      query->GetRestrictedToOfflineIds();
  EXPECT_EQ(Requirement::INCLUDE_MATCHING, offline_id_restriction.first);

  ASSERT_EQ(ids.size(), offline_id_restriction.second.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, offline_id_restriction.second.count(id))
        << "Did not find " << id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, OfflinePageIdsReplace) {
  std::vector<int64_t> ids = {1, 4, 9, 16};
  std::vector<int64_t> ids2 = {1, 2, 3, 4};

  builder_.SetOfflinePageIds(Requirement::INCLUDE_MATCHING, ids);
  builder_.SetOfflinePageIds(Requirement::INCLUDE_MATCHING, ids2);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::set<int64_t>> offline_id_restriction =
      query->GetRestrictedToOfflineIds();
  EXPECT_EQ(Requirement::INCLUDE_MATCHING, offline_id_restriction.first);

  ASSERT_EQ(ids2.size(), offline_id_restriction.second.size());
  for (auto id : ids2) {
    EXPECT_EQ(1U, offline_id_restriction.second.count(id))
        << "Did not find " << id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, ClientIdsSet) {
  std::vector<ClientId> ids = {kClientId2, {"invalid", "client id"}};
  builder_.SetClientIds(Requirement::INCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToClientIds();
  const Requirement& requirement = restriction.first;
  const std::set<ClientId>& ids_out = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);

  ASSERT_EQ(ids.size(), ids_out.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, ids_out.count(id)) << "Did not find " << id.name_space << "."
                                     << id.id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(kTestItem1));
}

TEST_F(OfflinePageModelQueryTest, ClientIdsSet_Exclude) {
  std::vector<ClientId> ids = {kClientId2, {"invalid", "client id"}};
  builder_.SetClientIds(Requirement::EXCLUDE_MATCHING, ids);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToClientIds();
  const Requirement& requirement = restriction.first;
  const std::set<ClientId>& ids_out = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);

  ASSERT_EQ(ids.size(), ids_out.size());
  for (auto id : ids) {
    EXPECT_EQ(1U, ids_out.count(id)) << "Did not find " << id.name_space << "."
                                     << id.id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, ClientIdsReplace) {
  std::vector<ClientId> ids = {kClientId2, {"invalid", "client id"}};
  std::vector<ClientId> ids2 = {kClientId1, {"invalid", "client id"}};

  builder_.SetClientIds(Requirement::INCLUDE_MATCHING, ids);
  builder_.SetClientIds(Requirement::INCLUDE_MATCHING, ids2);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToClientIds();
  const Requirement& requirement = restriction.first;
  const std::set<ClientId>& ids_out = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);

  ASSERT_EQ(ids2.size(), ids_out.size());
  for (auto id : ids2) {
    EXPECT_EQ(1U, ids_out.count(id)) << "Did not find " << id.name_space << "."
                                     << id.id << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, false);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, params.mode);
  EXPECT_FALSE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kUrl1, kTempUrl)));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kTempUrl, kUrl1)));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(GURL(""), GURL("https://abc.def"))));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_Exclude) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::EXCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, false);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, params.mode);
  EXPECT_FALSE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kUrl1, kTempUrl)));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kTempUrl, kUrl1)));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(GURL(""), GURL("https://abc.def"))));
}

TEST_F(OfflinePageModelQueryTest, UrlsReplace) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  std::vector<GURL> urls2 = {kUrl2, GURL("https://abc.def")};

  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, false);
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls2,
                   URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, false);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, params.mode);
  EXPECT_FALSE(params.strip_fragment);

  ASSERT_EQ(urls2.size(), params.urls.size());
  for (auto url : urls2) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
}

TEST_F(OfflinePageModelQueryTest, RequireRemovedOnCacheReset_Only) {
  builder_.RequireRemovedOnCacheReset(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsRemovedOnCacheReset(name_space))
        << "Namespace: " << name_space;
  }
  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(cache_page()));
  EXPECT_FALSE(query->Matches(download_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireRemovedOnCacheReset_Except) {
  builder_.RequireRemovedOnCacheReset(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsRemovedOnCacheReset(name_space))
        << "Namespace: " << name_space;
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(cache_page()));
  EXPECT_TRUE(query->Matches(download_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireSupportedByDownload_Only) {
  builder_.RequireSupportedByDownload(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsSupportedByDownload(name_space)) << "Namespace: "
                                                           << name_space;
  }
  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(download_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireSupportedByDownload_Except) {
  builder_.RequireSupportedByDownload(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsSupportedByDownload(name_space)) << "Namespace: "
                                                            << name_space;
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(download_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireShownAsRecentlyVisitedSite_Only) {
  builder_.RequireShownAsRecentlyVisitedSite(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsShownAsRecentlyVisitedSite(name_space))
        << "Namespace: " << name_space;
  }
  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(recent_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireShownAsRecentlyVisitedSite_Except) {
  builder_.RequireShownAsRecentlyVisitedSite(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsShownAsRecentlyVisitedSite(name_space))
        << "Namespace: " << name_space;
  }
  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(recent_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireRestrictedToOriginalTab_Only) {
  builder_.RequireRestrictedToOriginalTab(Requirement::INCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_TRUE(policy_.IsRestrictedToOriginalTab(name_space)) << "Namespace: "
                                                               << name_space;
  }
  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(original_tab_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireRestrictedToOriginalTab_Except) {
  builder_.RequireRestrictedToOriginalTab(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  for (const std::string& name_space : namespaces_allowed) {
    EXPECT_FALSE(policy_.IsRestrictedToOriginalTab(name_space)) << "Namespace: "
                                                                << name_space;
  }
  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(original_tab_page()));
}

TEST_F(OfflinePageModelQueryTest, IntersectNamespaces) {
  // This should exclude last N, but include |kTestNamespace|.
  builder_.RequireRestrictedToOriginalTab(Requirement::INCLUDE_MATCHING)
      .RequireShownAsRecentlyVisitedSite(Requirement::EXCLUDE_MATCHING);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);

  EXPECT_TRUE(namespaces_allowed.count(kTestNamespace) == 1);
  EXPECT_FALSE(query->Matches(recent_page()));
}

TEST_F(OfflinePageModelQueryTest, RequireNamespace) {
  builder_.RequireNamespace(kDefaultNamespace);
  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  auto restriction = query->GetRestrictedToNamespaces();
  std::set<std::string> namespaces_allowed = restriction.second;
  bool restricted_to_namespaces = restriction.first;
  EXPECT_TRUE(restricted_to_namespaces);
  EXPECT_EQ(1U, namespaces_allowed.size());
  EXPECT_TRUE(namespaces_allowed.find(kDefaultNamespace) !=
              namespaces_allowed.end());

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(test_namespace_page()));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_SearchByAll) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_ALL_URLS, false);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_ALL_URLS, params.mode);
  EXPECT_FALSE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kUrl1, kTempUrl)));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kTempUrl, kUrl1)));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(kUrl1, GURL("https://abc.def"))));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(kTempUrl, GURL("https://abc.def"))));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(GURL("https://abc.def"), GURL(""))));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kTempUrl, GURL(""))));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_Exclude_SearchByAll) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::EXCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_ALL_URLS, false);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_ALL_URLS, params.mode);
  EXPECT_FALSE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kUrl1, kTempUrl)));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kTempUrl, kUrl1)));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(kUrl1, GURL("https://abc.def"))));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(kTempUrl, GURL("https://abc.def"))));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(GURL("https://abc.def"), GURL(""))));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kTempUrl, GURL(""))));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_Defrag) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, true);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, params.mode);
  EXPECT_TRUE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kFragUrl1, kTempUrl)));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kTempUrl, kUrl1)));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(kFragUrl1, GURL("https://abc.def"))));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(kTempUrl, GURL("https://abc.def"))));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(GURL("https://abc.def"), GURL(""))));
  EXPECT_TRUE(query->Matches(
      CreatePageWithUrls(GURL("https://abc.def#frag2"), GURL(""))));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_Exclude_Defrag) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::EXCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, true);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_FINAL_URL_ONLY, params.mode);
  EXPECT_TRUE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kFragUrl1, kTempUrl)));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kTempUrl, kUrl1)));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(kFragUrl1, GURL("https://abc.def"))));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(kTempUrl, GURL("https://abc.def"))));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(GURL("https://abc.def"), GURL(""))));
  EXPECT_FALSE(query->Matches(
      CreatePageWithUrls(GURL("https://abc.def#frag2"), GURL(""))));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_SearchByAll_Defrag) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::INCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_ALL_URLS, true);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::INCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_ALL_URLS, params.mode);
  EXPECT_TRUE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(kTestItem2));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kFragUrl1, kTempUrl)));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kTempUrl, kFragUrl1)));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(kFragUrl1, GURL("https://abc.def"))));
  EXPECT_FALSE(query->Matches(
      CreatePageWithUrls(kTempUrl, GURL("https://abc.def#frag2"))));
  EXPECT_TRUE(
      query->Matches(CreatePageWithUrls(GURL("https://abc.def"), GURL(""))));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kTempFragUrl, GURL(""))));
}

TEST_F(OfflinePageModelQueryTest, UrlsSet_Exclude_SearchByAll_Defrag) {
  std::vector<GURL> urls = {kUrl1, GURL("https://abc.def")};
  builder_.SetUrls(Requirement::EXCLUDE_MATCHING, urls,
                   URLSearchMode::SEARCH_BY_ALL_URLS, true);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);

  auto restriction = query->GetRestrictedToUrls();
  const Requirement& requirement = restriction.first;
  const URLSearchParams& params = restriction.second;

  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, requirement);
  EXPECT_EQ(URLSearchMode::SEARCH_BY_ALL_URLS, params.mode);
  EXPECT_TRUE(params.strip_fragment);

  ASSERT_EQ(urls.size(), params.urls.size());
  for (auto url : urls) {
    EXPECT_EQ(1U, params.urls.count(url))
        << "Did not find " << url << "in query restrictions.";
  }

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(kTestItem2));
  EXPECT_FALSE(query->Matches(CreatePageWithUrls(kFragUrl1, kTempFragUrl)));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kTempUrl, kFragUrl1)));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(kUrl1, GURL("https://abc.def#frag2"))));
  EXPECT_TRUE(query->Matches(
      CreatePageWithUrls(kTempUrl, GURL("https://abc.def#frag2"))));
  EXPECT_FALSE(
      query->Matches(CreatePageWithUrls(GURL("https://abc.def"), GURL(""))));
  EXPECT_TRUE(query->Matches(CreatePageWithUrls(kTempFragUrl, GURL(""))));
}

TEST_F(OfflinePageModelQueryTest, RequestOrigin_Exclude) {
  builder_.SetRequestOrigin(Requirement::EXCLUDE_MATCHING, "");

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::string> offline_origin_restriction =
      query->GetRequestOrigin();
  EXPECT_EQ(Requirement::EXCLUDE_MATCHING, offline_origin_restriction.first);

  ASSERT_EQ("", offline_origin_restriction.second);

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(cct_page()));
}

TEST_F(OfflinePageModelQueryTest, RequestOrigin_Include) {
  builder_.SetRequestOrigin(Requirement::INCLUDE_MATCHING, "");

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::string> offline_origin_restriction =
      query->GetRequestOrigin();
  EXPECT_EQ(Requirement::INCLUDE_MATCHING, offline_origin_restriction.first);

  ASSERT_EQ("", offline_origin_restriction.second);

  EXPECT_TRUE(query->Matches(kTestItem1));
  EXPECT_FALSE(query->Matches(cct_page()));
}

TEST_F(OfflinePageModelQueryTest, RequestOrigin_Replace) {
  std::string origin1 = "";
  std::string origin2 = "[\"abc.xyz\",[\"12345\"]]";

  builder_.SetRequestOrigin(Requirement::INCLUDE_MATCHING, origin1);
  builder_.SetRequestOrigin(Requirement::INCLUDE_MATCHING, origin2);

  std::unique_ptr<OfflinePageModelQuery> query = builder_.Build(&policy_);
  std::pair<Requirement, std::string> offline_origin_restriction =
      query->GetRequestOrigin();
  EXPECT_EQ(Requirement::INCLUDE_MATCHING, offline_origin_restriction.first);

  ASSERT_EQ(origin2, offline_origin_restriction.second);

  EXPECT_FALSE(query->Matches(kTestItem1));
  EXPECT_TRUE(query->Matches(cct_page()));
}

}  // namespace offline_pages
