// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SEARCH_SERVICE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SEARCH_SERVICE_H_

#include <string>
#include <vector>

#include "base/containers/mru_cache.h"
#include "components/enhanced_bookmarks/bookmark_server_service.h"
#include "net/url_request/url_fetcher.h"

namespace enhanced_bookmarks {

class EnhancedBookmarkModel;

// Sends requests to the bookmark server to search for bookmarks relevant to a
// given query. Will handle one outgoing request at a time.
class BookmarkServerSearchService : public BookmarkServerService {
 public:
  BookmarkServerSearchService(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      EnhancedBookmarkModel* bookmark_model);
  ~BookmarkServerSearchService() override;

  // Triggers a search. The query must not be empty. A call to this method
  // cancels any previous searches. If there have been multiple queries in
  // between, onChange will only be called for the last query.
  // Note this method will be synchronous if query hits the cache.
  void Search(const std::string& query);

  // Returns search results for a query. Results for a query are only available
  // after Search() is called and after then OnChange() observer methods has
  // been sent.This method might return an empty vector, meaning there are no
  // bookmarks matching the given query. Returning null means we are still
  // loading and no results have come to the client. Previously cancelled
  // queries will not trigger onChange(), and this method will also return null
  // for queries that have never been passed to Search() before.
  scoped_ptr<std::vector<const bookmarks::BookmarkNode*>> ResultForQuery(
      const std::string& query);

 protected:
  scoped_ptr<net::URLFetcher> CreateFetcher() override;

  bool ProcessResponse(const std::string& response,
                       bool* should_notify) override;

  void CleanAfterFailure() override;

  // EnhancedBookmarkModelObserver methods.
  void EnhancedBookmarkModelLoaded() override{};
  void EnhancedBookmarkAdded(const bookmarks::BookmarkNode* node) override;
  void EnhancedBookmarkRemoved(const bookmarks::BookmarkNode* node) override {}
  void EnhancedBookmarkNodeChanged(
      const bookmarks::BookmarkNode* node) override {}
  void EnhancedBookmarkAllUserNodesRemoved() override;
  void EnhancedBookmarkRemoteIdChanged(const bookmarks::BookmarkNode* node,
                                       const std::string& old_remote_id,
                                       const std::string& remote_id) override;

 private:
  // Cache for previous search result, a map from a query string to vector of
  // star_ids.
  base::MRUCache<std::string, std::vector<std::string>> cache_;
  // The query currently on the fly, and is cleared as soon as the result is
  // available.
  std::string current_query_;
  DISALLOW_COPY_AND_ASSIGN(BookmarkServerSearchService);
};

}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SEARCH_SERVICE_H_
