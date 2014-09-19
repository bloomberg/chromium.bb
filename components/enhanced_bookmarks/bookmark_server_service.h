// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SERVICE_H_
#define COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SERVICE_H_

#include <string>
#include <vector>

#include "components/enhanced_bookmarks/enhanced_bookmark_model_observer.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"

class ProfileOAuth2TokenService;
class SigninManagerBase;
class BookmarkNode;

namespace enhanced_bookmarks {

class BookmarkServerService;
class EnhancedBookmarkModel;

class BookmarkServerServiceObserver {
 public:
  virtual void OnChange(BookmarkServerService* service) = 0;

 protected:
  virtual ~BookmarkServerServiceObserver() {}
};

// This abstract class manages the connection to the bookmark servers and
// stores the maps necessary to translate the response from stars.id to
// BookmarkNodes. Subclasses just have to provide the right query and the
// parsing of the response.
class BookmarkServerService : protected net::URLFetcherDelegate,
                              private OAuth2TokenService::Consumer,
                              public EnhancedBookmarkModelObserver {
 public:
  BookmarkServerService(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      ProfileOAuth2TokenService* token_service,
      SigninManagerBase* signin_manager,
      EnhancedBookmarkModel* enhanced_bookmark_model);
  virtual ~BookmarkServerService();

  void AddObserver(BookmarkServerServiceObserver* observer);
  void RemoveObserver(BookmarkServerServiceObserver* observer);

 protected:
  // Retrieves a bookmark by using its remote id. Returns null if nothing
  // matches.
  virtual const BookmarkNode* BookmarkForRemoteId(
      const std::string& remote_id) const;
  const std::string RemoteIDForBookmark(const BookmarkNode* bookmark) const;

  // Notifies the observers that something changed.
  void Notify();

  // Triggers a fetch.
  void TriggerTokenRequest(bool cancel_previous);

  // Build the query to send to the server. Returns a newly created url_fetcher.
  virtual net::URLFetcher* CreateFetcher() = 0;

  // Processes the response to the query. Returns true on successful parsing,
  // false on failure. The implementation can assume that |should_notify| is set
  // to true by default, if changed to false there will be no OnChange
  // notification send.
  virtual bool ProcessResponse(const std::string& response,
                               bool* should_notify) = 0;

  // If the token can't be retrieved or the query fails this method is called.
  virtual void CleanAfterFailure() = 0;

  // EnhancedBookmarkModelObserver:
  virtual void EnhancedBookmarkModelShuttingDown() OVERRIDE;

  SigninManagerBase* GetSigninManager();

  // Cached pointer to the bookmarks model.
  EnhancedBookmarkModel* model_;  // weak

 private:
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest, Cluster);
  FRIEND_TEST_ALL_PREFIXES(BookmarkServerServiceTest,
                           ClearClusterMapOnRemoveAllBookmarks);

  // net::URLFetcherDelegate methods. Called when the query is finished.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // OAuth2TokenService::Consumer methods.
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // The observers.
  ObserverList<BookmarkServerServiceObserver> observers_;
  // The Auth service is used to get a token for auth with the server.
  ProfileOAuth2TokenService* token_service_;  // Weak
  // The request to the token service.
  scoped_ptr<OAuth2TokenService::Request> token_request_;
  // To get the currently signed in user.
  SigninManagerBase* signin_manager_;  // Weak
  // To have access to the right context getter for the profile.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  // The fetcher used to query the server.
  scoped_ptr<net::URLFetcher> url_fetcher_;
  // A map from stars.id to bookmark nodes. With no null entries.
  std::map<std::string, const BookmarkNode*> starsid_to_bookmark_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkServerService);
};
}  // namespace enhanced_bookmarks

#endif  // COMPONENTS_ENHANCED_BOOKMARKS_BOOKMARK_SERVER_SERVICE_H_
