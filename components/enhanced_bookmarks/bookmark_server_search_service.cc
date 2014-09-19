// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/bookmark_server_search_service.h"

#include "components/enhanced_bookmarks/enhanced_bookmark_utils.h"
#include "components/enhanced_bookmarks/proto/search.pb.h"
#include "net/base/url_util.h"
#include "net/url_request/url_fetcher.h"

namespace {
const std::string kSearchUrl(
    "https://www.google.com/stars/search");
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
                            enhanced_bookmark_model) {
}

BookmarkServerSearchService::~BookmarkServerSearchService() {
}

void BookmarkServerSearchService::Search(const std::string& query) {
  DCHECK(query.length());
  current_query_ = query;
  TriggerTokenRequest(true);
}

std::vector<const BookmarkNode*> BookmarkServerSearchService::ResultForQuery(
    const std::string& query) {
  DCHECK(query.length());
  std::vector<const BookmarkNode*> result;

  std::map<std::string, std::vector<std::string> >::iterator it =
      searches_.find(query);
  if (it == searches_.end())
    return result;

  for (std::vector<std::string>::iterator clip_it = it->second.begin();
       clip_it != it->second.end();
       ++clip_it) {
    const BookmarkNode* node = BookmarkForRemoteId(*clip_it);
    if (node)
      result.push_back(node);
  }
  return result;
}

net::URLFetcher* BookmarkServerSearchService::CreateFetcher() {
  // Add the necessary arguments to the URI.
  GURL url(kSearchUrl);
  url = net::AppendQueryParameter(url, "output", "proto");
  url = net::AppendQueryParameter(url, "q", current_query_);

  // Build the URLFetcher to perform the request.
  net::URLFetcher* url_fetcher =
      net::URLFetcher::Create(url, net::URLFetcher::GET, this);

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
  for (google::protobuf::RepeatedPtrField<
           image::collections::CorpusSearchResult_ClipResult>::const_iterator
           it = response_proto.results().begin();
       it != response_proto.results().end();
       ++it) {
    const std::string& clip_id = it->clip_id();
    if (!clip_id.length())
      continue;
    clip_ids.push_back(clip_id);
  }
  searches_[current_query_] = clip_ids;
  current_query_.clear();
  return true;
}

void BookmarkServerSearchService::CleanAfterFailure() {
  searches_.clear();
}

void BookmarkServerSearchService::EnhancedBookmarkAdded(
    const BookmarkNode* node) {
  searches_.clear();
}

void BookmarkServerSearchService::EnhancedBookmarkAllUserNodesRemoved() {
  searches_.clear();
}

void BookmarkServerSearchService::EnhancedBookmarkRemoteIdChanged(
    const BookmarkNode* node,
    const std::string& old_remote_id,
    const std::string& remote_id) {
  searches_.clear();
}
}  // namespace enhanced_bookmarks
