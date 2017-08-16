// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_QUERY_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_QUERY_H_

#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_types.h"

namespace offline_pages {

// Can be used by OfflinePageModel instances to direct a query of the model.
class OfflinePageModelQuery {
 public:
  // A query can be constrained by several fields.  This constraint can be
  // either a positive or negative one (or no constraint): a page MUST have
  // something, or a page MUST NOT have something to match a query.
  enum class Requirement {
    // No requirement.
    UNSET,
    // All returned pages will have one of the values specified in the query
    // requirement.
    INCLUDE_MATCHING,
    // All returned pages will not have any of the values specified in the query
    // requirement.
    EXCLUDE_MATCHING,
  };

  struct URLSearchParams {
    URLSearchParams();
    URLSearchParams(const URLSearchParams& params);
    URLSearchParams(std::set<GURL> url_set,
                    URLSearchMode search_mode,
                    bool strip_frag);
    ~URLSearchParams();

    // The set of urls for matching.
    std::set<GURL> urls;
    // The mode for searching. By final url only or both final and original
    // urls.
    URLSearchMode mode;
    // Whether fragments should be stripped. It will *not* work on
    // *|original_url|*.
    // TODO(crbug.com/753609): Try to make this also available when matching for
    // |original_url|.
    bool strip_fragment;
  };

  OfflinePageModelQuery();
  virtual ~OfflinePageModelQuery();

  std::pair<bool, std::set<std::string>> GetRestrictedToNamespaces() const;
  std::pair<Requirement, std::set<int64_t>> GetRestrictedToOfflineIds() const;
  std::pair<Requirement, std::set<ClientId>> GetRestrictedToClientIds() const;
  std::pair<Requirement, URLSearchParams> GetRestrictedToUrls() const;
  std::pair<Requirement, std::string> GetRequestOrigin() const;

  // This is the workhorse function that is used by the in-memory offline page
  // model, given a page it will find out whether that page matches the query.
  bool Matches(const OfflinePageItem& page) const;

 private:
  friend class OfflinePageModelQueryBuilder;

  std::unique_ptr<std::set<std::string>> restricted_to_namespaces_;

  std::pair<Requirement, std::set<int64_t>> offline_ids_;
  std::pair<Requirement, std::set<ClientId>> client_ids_;
  std::pair<Requirement, URLSearchParams> urls_;
  std::pair<Requirement, std::string> request_origin_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelQuery);
};

// Used to create an offline page model query.  QueryBuilders without
// modifications create queries that allow all pages.
// Can restrict results by policies provided by |ClientPolicyController|, or by
// individual features of pages. Each restriction comes with a |Requirement|
// that can be used to specify whether the input restriction should include or
// exclude matching pages.
class OfflinePageModelQueryBuilder {
 public:
  using Requirement = OfflinePageModelQuery::Requirement;

  OfflinePageModelQueryBuilder();
  ~OfflinePageModelQueryBuilder();

  // Sets the offline page IDs that are valid for this request.  If called
  // multiple times, overwrites previous offline page ID restrictions.
  OfflinePageModelQueryBuilder& SetOfflinePageIds(
      Requirement requirement,
      const std::vector<int64_t>& ids);

  // Sets the client IDs that are valid for this request.  If called multiple
  // times, overwrites previous client ID restrictions.
  OfflinePageModelQueryBuilder& SetClientIds(Requirement requirement,
                                             const std::vector<ClientId>& ids);

  // Sets the request origin that are valid for this request. If called
  // multiple times, overwrites the previous request origin restrictions.
  OfflinePageModelQueryBuilder& SetRequestOrigin(
      Requirement requirement,
      const std::string& request_origin);

  // Sets the URLs that are valid for this request.  If called multiple times,
  // overwrites previous URL restrictions.
  // |search_mode| is used to control if the URL will be matched with final
  // URL only or both final URL and original URL.
  // If |strip_fragment| is true, *only final* urls will be matched without
  // fragment.
  // TODO(crbug.com/753609): Try to unify fragment handling for original and
  // final urls.
  OfflinePageModelQueryBuilder& SetUrls(Requirement requirement,
                                        const std::vector<GURL>& urls,
                                        URLSearchMode search_mode,
                                        bool strip_fragment);

  // Only include pages whose namespaces satisfy
  // ClientPolicyController::IsRemovedOnCacheReset(|namespace|) ==
  //     |removed_on_cache_reset|
  // Multiple calls overwrite previous ones.
  OfflinePageModelQueryBuilder& RequireRemovedOnCacheReset(
      Requirement removed_on_cache_reset);

  // Only include pages whose namespaces satisfy
  // ClientPolicyController::IsSupportedByDownload(|namespace|) ==
  //     |supported_by_download|
  // Multiple calls overwrite previous ones.
  OfflinePageModelQueryBuilder& RequireSupportedByDownload(
      Requirement supported_by_download);

  // Only include pages whose namespaces satisfy
  // ClientPolicyController::IsShownAsRecentlyVisitedSite(|namespace|) ==
  //     |recently_visited|
  // Multiple calls overwrite previous ones.
  OfflinePageModelQueryBuilder& RequireShownAsRecentlyVisitedSite(
      Requirement recently_visited);

  // Only include pages whose namespaces satisfy
  // ClientPolicyController::IsRestrictedToOriginalTab(|namespace|) ==
  //     |original_tab|
  // Multiple calls overwrite previous ones.
  OfflinePageModelQueryBuilder& RequireRestrictedToOriginalTab(
      Requirement original_tab);

  // Only include results from a single namespace.
  OfflinePageModelQueryBuilder& RequireNamespace(const std::string& name_space);

  // Builds the query using the namespace policies provided by |controller|
  // This resets the internal state.  |controller| should not be |nullptr|.
  std::unique_ptr<OfflinePageModelQuery> Build(
      ClientPolicyController* controller);

 private:
  // Intersects the allowed namespaces in query_ with |namespaces|.  If
  // |inverted| is true, intersects the allowed namespaces with all namespaces
  // except those provided in |namespaces|.
  void IntersectWithNamespaces(ClientPolicyController* controller,
                               const std::vector<std::string>& namespaces,
                               Requirement match_requirement);

  std::pair<Requirement, std::vector<int64_t>> offline_ids_;
  std::pair<Requirement, std::vector<ClientId>> client_ids_;
  std::pair<Requirement, OfflinePageModelQuery::URLSearchParams> urls_;
  std::pair<Requirement, std::string> request_origin_;

  Requirement removed_on_cache_reset_ = Requirement::UNSET;
  Requirement supported_by_download_ = Requirement::UNSET;
  Requirement shown_as_recently_visited_site_ = Requirement::UNSET;
  Requirement restricted_to_original_tab_ = Requirement::UNSET;

  std::unique_ptr<std::string> name_space_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageModelQueryBuilder);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_MODEL_QUERY_H_
