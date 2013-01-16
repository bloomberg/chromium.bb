// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/url_request/url_request_context_getter.h"

class GoogleServiceAuthError;
class OAuth2AccessTokenConsumer;
class Profile;

// OAuth2TokenService is a ProfileKeyedService that retrieves OAuth2 access
// tokens for a given set of scopes using the OAuth2 refresh token maintained by
// TokenService. All calls are expected from the UI thread.
//
// To use this service, call StartRequest() with a given set of scopes and a
// consumer of the request results. The consumer is required to outlive the
// request. The request can be deleted. The consumer may be called back
// asynchronously with the fetch results.
//
// - If the consumer is not called back before the request is deleted, it will
//   never be called back.
//   Note in this case, the actual network requests are not canceled and the
//   cache will be populated with the fetched results; it is just the consumer
//   callback that is aborted.
//
// - Otherwise the consumer will be called back with the request and the fetch
//   results.
//
// The caller of StartRequest() owns the returned request and is responsible to
// delete the request even once the callback has been invoked.
//
// Note the request should be started from the UI thread. To start a request
// from other thread, please use OAuth2TokenServiceRequest.
class OAuth2TokenService : public content::NotificationObserver,
                           public SigninGlobalError::AuthStatusProvider,
                           public ProfileKeyedService {
 public:
  // Class representing a request that fetches an OAuth2 access token.
  class Request {
   public:
    virtual ~Request();
   protected:
    Request();
  };

  // Class representing the consumer of a Request passed to |StartRequest|,
  // which will be called back when the request completes.
  class Consumer {
   public:
    Consumer();
    virtual ~Consumer();
    // |request| is a Request that is started by this consumer and has
    // completed.
    virtual void OnGetTokenSuccess(const Request* request,
                                   const std::string& access_token,
                                   const base::Time& expiration_time) = 0;
    virtual void OnGetTokenFailure(const Request* request,
                                   const GoogleServiceAuthError& error) = 0;
  };

  // A set of scopes in OAuth2 authentication.
  typedef std::set<std::string> ScopeSet;

  OAuth2TokenService();
  virtual ~OAuth2TokenService();

  // Initializes this token service with the profile.
  void Initialize(Profile* profile);

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // Starts a request for an OAuth2 access token using the OAuth2 refresh token
  // maintained by TokenService. The caller owns the returned Request. |scopes|
  // is the set of scopes to get an access token for, |consumer| is the object
  // that will be called back with results if the returned request is not
  // deleted.
  // Note the refresh token has been collected from TokenService when this
  // method returns, and the request can continue even if TokenService clears
  // its tokens after this method returns. This means that outstanding
  // StartRequest actions will still complete even if the user signs out in the
  // meantime.
  virtual scoped_ptr<Request> StartRequest(
      const ScopeSet& scopes,
      OAuth2TokenService::Consumer* consumer);

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SigninGlobalError::AuthStatusProvider implementation.
  virtual GoogleServiceAuthError GetAuthStatus() const OVERRIDE;

 private:
  // Class that fetches an OAuth2 access token for a given set of scopes and
  // OAuth2 refresh token.
  class Fetcher;
  friend class Fetcher;
  // Implementation of Request.
  class RequestImpl;

  // Informs the consumer of |request| fetch results.
  static void InformConsumer(
      base::WeakPtr<OAuth2TokenService::RequestImpl> request,
      GoogleServiceAuthError error,
      std::string access_token,
      base::Time expiration_date);

  // Struct that contains the information of an OAuth2 access token.
  struct CacheEntry {
    std::string access_token;
    base::Time expiration_date;
  };

  // Returns a currently valid OAuth2 access token for the given set of scopes,
  // or NULL if none have been cached. Note the user of this method should
  // ensure no entry with the same |scopes| is added before the usage of the
  // returned entry is done.
  const CacheEntry* GetCacheEntry(const ScopeSet& scopes);
  // Registers a new access token in the cache if |refresh_token| is the one
  // currently held by TokenService.
  void RegisterCacheEntry(const std::string& refresh_token,
                          const ScopeSet& scopes,
                          const std::string& access_token,
                          const base::Time& expiration_date);

  // Called when |fetcher| finishes fetching.
  void OnFetchComplete(Fetcher* fetcher);

  // Updates the internal cache of the result from the most-recently-completed
  // auth request (used for reporting errors to the user).
  void UpdateAuthError(const GoogleServiceAuthError& error);

  // The profile with which this instance was initialized, or NULL.
  Profile* profile_;

  // The auth status from the most-recently-completed request.
  GoogleServiceAuthError last_auth_error_;

  // Getter to use for fetchers.
  scoped_refptr<net::URLRequestContextGetter> getter_;

  // The cache of currently valid tokens.
  typedef std::map<ScopeSet, CacheEntry> TokenCache;
  TokenCache token_cache_;

  // The parameters (refresh token and scope set) used to fetch an OAuth2 access
  // token.
  typedef std::pair<std::string, ScopeSet> FetchParameters;
  // A map from fetch parameters to a fetcher that is fetching an OAuth2 access
  // token using these parameters.
  std::map<FetchParameters, Fetcher*> pending_fetchers_;

  // Registrar for notifications from the TokenService.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_H_
