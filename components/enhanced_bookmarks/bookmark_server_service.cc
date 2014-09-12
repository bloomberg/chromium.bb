// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enhanced_bookmarks/bookmark_server_service.h"

#include "base/auto_reset.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_model_observer.h"
#include "components/enhanced_bookmarks/metadata_accessor.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/models/tree_node_iterator.h"

namespace enhanced_bookmarks {

BookmarkServerService::BookmarkServerService(
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    ProfileOAuth2TokenService* token_service,
    SigninManagerBase* signin_manager,
    BookmarkModel* bookmark_model)
    : OAuth2TokenService::Consumer("bookmark_server_service"),
      bookmark_model_(bookmark_model),
      token_service_(token_service),
      signin_manager_(signin_manager),
      request_context_getter_(request_context_getter),
      inhibit_change_notifications_(false) {
  DCHECK(request_context_getter.get());
  DCHECK(token_service);
  DCHECK(signin_manager);
  DCHECK(bookmark_model);
  bookmark_model_->AddObserver(this);
  if (bookmark_model_->loaded())
    BuildIdMap();
}

BookmarkServerService::~BookmarkServerService() {
  bookmark_model_->RemoveObserver(this);
}

void BookmarkServerService::AddObserver(
    BookmarkServerServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void BookmarkServerService::RemoveObserver(
    BookmarkServerServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

void BookmarkServerService::BuildIdMap() {
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      bookmark_model_->root_node());

  while (iterator.has_next()) {
    const BookmarkNode* bookmark = iterator.Next();
    if (bookmark_model_->is_permanent_node(bookmark))
      continue;
    // RemoteIdFromBookmark() will create the ID if it doesn't exists yet.
    std::string starid =
        enhanced_bookmarks::RemoteIdFromBookmark(bookmark_model_, bookmark);
    if (bookmark->is_url()) {
      starsid_to_bookmark_[starid] = bookmark;
    }
  }
}

const BookmarkNode* BookmarkServerService::BookmarkForRemoteId(
    const std::string& remote_id) const {
  std::map<std::string, const BookmarkNode*>::const_iterator it =
      starsid_to_bookmark_.find(remote_id);
  if (it == starsid_to_bookmark_.end())
    return NULL;
  return it->second;
}

const std::string BookmarkServerService::RemoteIDForBookmark(
    const BookmarkNode* bookmark) const {
  return enhanced_bookmarks::RemoteIdFromBookmark(bookmark_model_, bookmark);
}

void BookmarkServerService::Notify() {
  FOR_EACH_OBSERVER(BookmarkServerServiceObserver, observers_, OnChange(this));
}

void BookmarkServerService::TriggerTokenRequest(bool cancel_previous) {
  if (cancel_previous)
    url_fetcher_.reset();

  if (token_request_ || url_fetcher_)
    return;  // Fetcher is already running.

  const std::string username(signin_manager_->GetAuthenticatedUsername());
  if (!username.length()) {
    // User is not signed in.
    CleanAfterFailure();
    Notify();
    return;
  }
  // Find a token.
  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  token_request_ = token_service_->StartRequest(username, scopes, this);
}

//
// OAuth2AccessTokenConsumer methods.
//
void BookmarkServerService::OnGetTokenSuccess(
    const OAuth2TokenService::Request* request,
    const std::string& access_token,
    const base::Time& expiration_time) {
  url_fetcher_.reset(CreateFetcher());
  // Free the token request.
  token_request_.reset();

  if (!url_fetcher_) {
    CleanAfterFailure();
    Notify();
    return;
  }
  url_fetcher_->SetRequestContext(request_context_getter_.get());

  // Add the token.
  std::string headers;
  headers = "Authorization: Bearer ";
  headers += access_token;
  headers += "\r\n";
  url_fetcher_->SetExtraRequestHeaders(headers);

  // Do not pollute the cookie store with cruft, or mix the users cookie in this
  // request.
  url_fetcher_->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                             net::LOAD_DO_NOT_SAVE_COOKIES);

  url_fetcher_->Start();
}

void BookmarkServerService::OnGetTokenFailure(
    const OAuth2TokenService::Request* request,
    const GoogleServiceAuthError& error) {
  // Free the request.
  token_request_.reset();
  CleanAfterFailure();
  Notify();
}

//
// net::URLFetcherDelegate methods.
//
void BookmarkServerService::OnURLFetchComplete(const net::URLFetcher* source) {
  scoped_ptr<net::URLFetcher> url_fetcher(url_fetcher_.Pass());
  std::string response;
  bool should_notify = true;

  if (url_fetcher->GetResponseCode() != 200 ||
      !url_fetcher->GetResponseAsString(&response) ||
      !ProcessResponse(response, &should_notify)) {
    CleanAfterFailure();
  }
  if (should_notify)
    Notify();
}

//
// BookmarkModelObserver methods.
//
void BookmarkServerService::BookmarkModelLoaded(BookmarkModel* model,
                                                bool ids_reassigned) {
  BuildIdMap();
}

void BookmarkServerService::BookmarkNodeAdded(BookmarkModel* model,
                                              const BookmarkNode* parent,
                                              int index) {
  DCHECK(!inhibit_change_notifications_);
  const BookmarkNode* bookmark = parent->GetChild(index);
  if (!bookmark->is_url())
    return;

  base::AutoReset<bool> inhibitor(&inhibit_change_notifications_, true);
  std::string starid =
      enhanced_bookmarks::RemoteIdFromBookmark(model, bookmark);
  starsid_to_bookmark_[starid] = bookmark;
}

void BookmarkServerService::BookmarkNodeRemoved(
    BookmarkModel* model,
    const BookmarkNode* parent,
    int old_index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls) {
  DCHECK(!inhibit_change_notifications_);
  if (!node->is_url())
    return;
  base::AutoReset<bool> inhibitor(&inhibit_change_notifications_, true);
  std::string starid = enhanced_bookmarks::RemoteIdFromBookmark(model, node);
  starsid_to_bookmark_.erase(starid);
}

void BookmarkServerService::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  if (!node->is_url() || inhibit_change_notifications_)
    return;
  base::AutoReset<bool> inhibitor(&inhibit_change_notifications_, true);
  std::string starid = enhanced_bookmarks::RemoteIdFromBookmark(model, node);
  starsid_to_bookmark_.erase(starid);
}

void BookmarkServerService::BookmarkMetaInfoChanged(BookmarkModel* model,
                                                    const BookmarkNode* node) {
  if (!node->is_url() || inhibit_change_notifications_)
    return;

  std::string starid = enhanced_bookmarks::RemoteIdFromBookmark(model, node);
  starsid_to_bookmark_[starid] = node;
}

void BookmarkServerService::BookmarkAllUserNodesRemoved(
    BookmarkModel* model,
    const std::set<GURL>& removed_urls) {
  DCHECK(!inhibit_change_notifications_);
  starsid_to_bookmark_.clear();
}

SigninManagerBase* BookmarkServerService::GetSigninManager() {
  DCHECK(signin_manager_);
  return signin_manager_;
}

}  // namespace enhanced_bookmarks
