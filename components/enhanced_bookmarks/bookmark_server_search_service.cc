// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/bookmark_server_search_service.h"

#include "components/enhanced_bookmarks/enhanced_bookmark_model.h"
#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"
#include "components/enhanced_bookmarks/proto/search.pb.h"
#include "net/base/url_util.h"
#include "net/url_request/url_fetcher.h"

using bookmarks::BookmarkNode;

namespace {
const char kSearchUrl[] = "https://www.google.com/stars/search";
const int kSearchCacheMaxSize = 50;
}  // namespace

namespace enhanced_bookmarks {

BookmarkServerSearchService::BookmarkServerSearchService(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    EnhancedBookmarkModel* enhanced_bookmark_model)
    : BookmarkServerService(request_context_getter,
                            token_service,
                            signin_manager,
                            enhanced_bookmark_model),
      cache_(kSearchCacheMaxSize) {
}

BookmarkServerSearchService::~BookmarkServerSearchService() {
}

void BookmarkServerSearchService::Search(const std::string& query) {
  DCHECK(query.length());
  if (current_query_ == query)
    return;

  // If result is already stored in cache, immediately notify observers.
  if (cache_.Get(current_query_) != cache_.end()) {
    Cancel();
    Notify();
    return;
  }
  current_query_ = query;
  TriggerTokenRequest(true);
}

scoped_ptr<std::vector<const BookmarkNode*>>
BookmarkServerSearchService::ResultForQuery(const std::string& query) {
  DCHECK(query.length());
  scoped_ptr<std::vector<const BookmarkNode*>> result;

  const auto& it = cache_.Get(query);
  if (it == cache_.end())
    return result;

  result.reset(new std::vector<const BookmarkNode*>());

  for (const std::string& clip_id : it->second) {
    const BookmarkNode* node = BookmarkForRemoteId(clip_id);
    if (node)
      result->push_back(node);
  }
  return result;
}

scoped_ptr<net::URLFetcher> BookmarkServerSearchService::CreateFetcher() {
  // Add the necessary arguments to the URI.
  GURL url(kSearchUrl);
  url = net::AppendQueryParameter(url, "output", "proto");
  url = net::AppendQueryParameter(url, "q", current_query_);
  url = net::AppendQueryParameter(url, "v", model_->GetVersionString());

  // Build the URLFetcher to perform the request.
  scoped_ptr<net::URLFetcher> url_fetcher(
      net::URLFetcher::Create(url, net::URLFetcher::GET, this));

  return url_fetcher;
}

bool BookmarkServerSearchService::ProcessResponse(const std::string& response,
                                                  bool* should_notify) {
  DCHECK(*should_notify);
  DCHECK(current_query_.length());
  image::collections::CorpusSearchResult response_proto;
  bool result = response_proto.ParseFromString(response);
  if (!result)
    return false;  // Not formatted properly.

  std::vector<std::string> clip_ids;
  for (const image::collections::CorpusSearchResult_ClipResult& clip_result :
       response_proto.results()) {
    const std::string& clip_id = clip_result.clip_id();
    if (!clip_id.length())
      continue;
    clip_ids.push_back(clip_id);
  }
  cache_.Put(current_query_, clip_ids);
  current_query_.clear();
  return true;
}

void BookmarkServerSearchService::CleanAfterFailure() {
  cache_.Clear();
  current_query_.clear();
}

void BookmarkServerSearchService::EnhancedBookmarkAdded(
    const BookmarkNode* node) {
  cache_.Clear();
}

void BookmarkServerSearchService::EnhancedBookmarkAllUserNodesRemoved() {
  cache_.Clear();
}

void BookmarkServerSearchService::EnhancedBookmarkRemoteIdChanged(
    const BookmarkNode* node,
    const std::string& old_remote_id,
    const std::string& remote_id) {
  cache_.Clear();
}
}  // namespace enhanced_bookmarks
