// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SEARCH_SERVICE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SEARCH_SERVICE_H_

#include <string>
#include <vector>

#include "components/bookmarks/browser/base_bookmark_model_observer.h"
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
  virtual ~BookmarkServerSearchService();

  // Triggers a search. The query must not be empty. A call to this method
  // cancels any previous searches. OnChange() is garanteed to be called once
  // per query.
  void Search(const std::string& query);

  // Returns the search results. The results are only available after the
  // OnChange() observer methods has been sent. This method will return an empty
  // result otherwise. query should be a string passed to Search() previously.
  std::vector<const BookmarkNode*> ResultForQuery(const std::string& query);

 protected:

  virtual net::URLFetcher* CreateFetcher() OVERRIDE;

  virtual bool ProcessResponse(const std::string& response,
                               bool* should_notify) OVERRIDE;

  virtual void CleanAfterFailure() OVERRIDE;

  // EnhancedBookmarkModelObserver methods.
  virtual void EnhancedBookmarkModelLoaded() OVERRIDE{};
  virtual void EnhancedBookmarkAdded(const BookmarkNode* node) OVERRIDE;
  virtual void EnhancedBookmarkRemoved(const BookmarkNode* node) OVERRIDE{};
  virtual void EnhancedBookmarkAllUserNodesRemoved() OVERRIDE;
  virtual void EnhancedBookmarkRemoteIdChanged(
      const BookmarkNode* node,
      const std::string& old_remote_id,
      const std::string& remote_id) OVERRIDE;

 private:
  // The search data, a map from query to a vector of stars.id.
  std::map<std::string, std::vector<std::string> > searches_;
  std::string current_query_;
  DISALLOW_COPY_AND_ASSIGN(BookmarkServerSearchService);
};
}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SEARCH_SERVICE_H_
