// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"

namespace base {
class Time;
}

namespace net {
class URLRequestContextGetter;
}

class GoogleServiceAuthError;

// Abstract base class for a service that fetches and caches OAuth2 access
// tokens. Concrete subclasses should implement GetRefreshToken to return
// the appropriate refresh token.
//
// All calls are expected from the UI thread.
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
class OAuth2TokenService {
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

  explicit OAuth2TokenService(net::URLRequestContextGetter* getter);
  virtual ~OAuth2TokenService();

  // Checks in the cache for a valid access token, and if not found starts
  // a request for an OAuth2 access token using the OAuth2 refresh token
  // maintained by this instance. The caller owns the returned Request.
  // |scopes| is the set of scopes to get an access token for, |consumer| is
  // the object that will be called back with results if the returned request
  // is not deleted.
  virtual scoped_ptr<Request> StartRequest(const ScopeSet& scopes,
                                           Consumer* consumer);

  // Returns true if a refresh token exists. If false, calls to
  // |StartRequest| will result in a Consumer::OnGetTokenFailure callback.
  bool RefreshTokenIsAvailable();

  // Mark an OAuth2 access token as invalid. This should be done if the token
  // was received from this class, but was not accepted by the server (e.g.,
  // the server returned 401 Unauthorized). The token will be removed from the
  // cache for the given scopes.
  virtual void InvalidateToken(const ScopeSet& scopes,
                               const std::string& invalid_token);

  // Return the current number of entries in the cache.
  int cache_size_for_testing() const;

 protected:
  // Subclasses should return the refresh token maintained.
  // If no token is available, return an empty string.
  virtual std::string GetRefreshToken() = 0;

  // Subclasses can override if they want to report errors to the user.
  virtual void UpdateAuthError(const GoogleServiceAuthError& error);

  // Add a new entry to the cache.
  // Subclasses can override if there are implementation-specific reasons
  // that an access token should ever not be cached.
  virtual void RegisterCacheEntry(const std::string& refresh_token,
                                  const ScopeSet& scopes,
                                  const std::string& access_token,
                                  const base::Time& expiration_date);

  // Returns true if GetCacheEntry would return a valid cache entry for the
  // given scopes.
  bool HasCacheEntry(const ScopeSet& scopes);

  // Posts a task to fire the Consumer callback with the cached token.  Must
  // only be called if HasCacheEntry() returns true.
  scoped_ptr<Request> StartCacheLookupRequest(const ScopeSet& scopes,
                                              Consumer* consumer);

  // Clears the internal token cache.
  void ClearCache();

  // Implements a cancelable |OAuth2TokenService::Request|, which should be
  // operated on the UI thread.
  class RequestImpl : public base::SupportsWeakPtr<RequestImpl>,
                      public Request {
   public:
    // |consumer| is required to outlive this.
    explicit RequestImpl(Consumer* consumer);
    virtual ~RequestImpl();

    // Informs |consumer_| that this request is completed.
    void InformConsumer(const GoogleServiceAuthError& error,
                        const std::string& access_token,
                        const base::Time& expiration_date);

   private:
    // |consumer_| to call back when this request completes.
    Consumer* const consumer_;
  };

  // Informs the consumer of |request| fetch results.
  static void InformConsumer(base::WeakPtr<RequestImpl> request,
                             const GoogleServiceAuthError& error,
                             const std::string& access_token,
                             const base::Time& expiration_date);

 private:
  // Class that fetches an OAuth2 access token for a given set of scopes and
  // OAuth2 refresh token.
  class Fetcher;
  friend class Fetcher;

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


  // Removes an access token for the given set of scopes from the cache.
  // Returns true if the entry was removed, otherwise false.
  bool RemoveCacheEntry(const OAuth2TokenService::ScopeSet& scopes,
                        const std::string& token_to_remove);


  // Called when |fetcher| finishes fetching.
  void OnFetchComplete(Fetcher* fetcher);

  // Getter to use for fetchers.
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // The cache of currently valid tokens.
  typedef std::map<ScopeSet, CacheEntry> TokenCache;
  TokenCache token_cache_;

  // The parameters (refresh token and scope set) used to fetch an OAuth2 access
  // token.
  typedef std::pair<std::string, ScopeSet> FetchParameters;
  // A map from fetch parameters to a fetcher that is fetching an OAuth2 access
  // token using these parameters.
  std::map<FetchParameters, Fetcher*> pending_fetchers_;

  DISALLOW_COPY_AND_ASSIGN(OAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_OAUTH2_TOKEN_SERVICE_H_
