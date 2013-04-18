// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/oauth2_token_service.h"

#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_consumer.h"
#include "google_apis/gaia/oauth2_access_token_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace {

// Maximum number of retries in fetching an OAuth2 access token.
const int kMaxFetchRetryNum = 5;

// Returns an exponential backoff in milliseconds including randomness less than
// 1000 ms when retrying fetching an OAuth2 access token.
int64 ComputeExponentialBackOffMilliseconds(int retry_num) {
  DCHECK(retry_num < kMaxFetchRetryNum);
  int64 exponential_backoff_in_seconds = 1 << retry_num;
  // Returns a backoff with randomness < 1000ms
  return (exponential_backoff_in_seconds + base::RandDouble()) * 1000;
}

}  // namespace

OAuth2TokenService::RequestImpl::RequestImpl(
    OAuth2TokenService::Consumer* consumer)
    : consumer_(consumer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

OAuth2TokenService::RequestImpl::~RequestImpl() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
}

void OAuth2TokenService::RequestImpl::InformConsumer(
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_date) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  if (error.state() == GoogleServiceAuthError::NONE)
    consumer_->OnGetTokenSuccess(this, access_token, expiration_date);
  else
    consumer_->OnGetTokenFailure(this, error);
}

// Class that fetches OAuth2 access tokens for given scopes and refresh token.
//
// It aims to meet OAuth2TokenService's requirements on token fetching. Retry
// mechanism is used to handle failures.
//
// To use this class, call CreateAndStart() to create and start a Fetcher.
//
// The Fetcher will call back the service by calling
// OAuth2TokenService::OnFetchComplete() when it completes fetching, if it is
// not destructed before it completes fetching; if the Fetcher is destructed
// before it completes fetching, the service will never be called back. The
// Fetcher destructs itself after calling back the service when finishes
// fetching.
//
// Requests that are waiting for the fetching results of this Fetcher can be
// added to the Fetcher by calling
// OAuth2TokenService::Fetcher::AddWaitingRequest() before the Fetcher completes
// fetching.
//
// The waiting requests are taken as weak pointers and they can be deleted. The
// waiting requests will be called back with fetching results if they are not
// deleted
// - when the Fetcher completes fetching, if the Fetcher is not destructed
//   before it completes fetching, or
// - when the Fetcher is destructed if the Fetcher is destructed before it
//   completes fetching (in this case, the waiting requests will be called back
//   with error).
class OAuth2TokenService::Fetcher : public OAuth2AccessTokenConsumer {
 public:
  // Creates a Fetcher and starts fetching an OAuth2 access token for
  // |refresh_token| and |scopes| in the request context obtained by |getter|.
  // The given |oauth2_token_service| will be informed when fetching is done.
  static Fetcher* CreateAndStart(OAuth2TokenService* oauth2_token_service,
                                 net::URLRequestContextGetter* getter,
                                 const std::string& refresh_token,
                                 const OAuth2TokenService::ScopeSet& scopes,
                                 base::WeakPtr<RequestImpl> waiting_request);
  virtual ~Fetcher();

  // Add a request that is waiting for the result of this Fetcher.
  void AddWaitingRequest(base::WeakPtr<RequestImpl> waiting_request);

  const OAuth2TokenService::ScopeSet& GetScopeSet() const;
  const std::string& GetRefreshToken() const;

  // The error result from this fetcher.
  const GoogleServiceAuthError& error() const { return error_; }

 protected:
   // OAuth2AccessTokenConsumer
  virtual void OnGetTokenSuccess(const std::string& access_token,
                                 const base::Time& expiration_date) OVERRIDE;
  virtual void OnGetTokenFailure(const GoogleServiceAuthError& error) OVERRIDE;

 private:
  Fetcher(OAuth2TokenService* oauth2_token_service,
          net::URLRequestContextGetter* getter,
          const std::string& refresh_token,
          const OAuth2TokenService::ScopeSet& scopes,
          base::WeakPtr<RequestImpl> waiting_request);
  void Start();
  void InformWaitingRequests();
  static bool ShouldRetry(const GoogleServiceAuthError& error);

  // |oauth2_token_service_| remains valid for the life of this Fetcher, since
  // this Fetcher is destructed in the dtor of the OAuth2TokenService or is
  // scheduled for deletion at the end of OnGetTokenFailure/OnGetTokenSuccess
  // (whichever comes first).
  OAuth2TokenService* const oauth2_token_service_;
  scoped_refptr<net::URLRequestContextGetter> getter_;
  const std::string refresh_token_;
  const OAuth2TokenService::ScopeSet scopes_;
  std::vector<base::WeakPtr<RequestImpl> > waiting_requests_;

  int retry_number_;
  base::OneShotTimer<OAuth2TokenService::Fetcher> retry_timer_;
  scoped_ptr<OAuth2AccessTokenFetcher> fetcher_;

  // Variables that store fetch results.
  // Initialized to be GoogleServiceAuthError::SERVICE_UNAVAILABLE to handle
  // destruction.
  GoogleServiceAuthError error_;
  std::string access_token_;
  base::Time expiration_date_;

  DISALLOW_COPY_AND_ASSIGN(Fetcher);
};

// static
OAuth2TokenService::Fetcher* OAuth2TokenService::Fetcher::CreateAndStart(
    OAuth2TokenService* oauth2_token_service,
    net::URLRequestContextGetter* getter,
    const std::string& refresh_token,
    const OAuth2TokenService::ScopeSet& scopes,
    base::WeakPtr<RequestImpl> waiting_request) {
  OAuth2TokenService::Fetcher* fetcher = new Fetcher(
      oauth2_token_service, getter, refresh_token, scopes, waiting_request);
  fetcher->Start();
  return fetcher;
}

OAuth2TokenService::Fetcher::Fetcher(
    OAuth2TokenService* oauth2_token_service,
    net::URLRequestContextGetter* getter,
    const std::string& refresh_token,
    const OAuth2TokenService::ScopeSet& scopes,
    base::WeakPtr<RequestImpl> waiting_request)
    : oauth2_token_service_(oauth2_token_service),
      getter_(getter),
      refresh_token_(refresh_token),
      scopes_(scopes),
      retry_number_(0),
      error_(GoogleServiceAuthError::SERVICE_UNAVAILABLE) {
  DCHECK(oauth2_token_service_);
  DCHECK(getter_);
  DCHECK(refresh_token_.length());
  waiting_requests_.push_back(waiting_request);
}

OAuth2TokenService::Fetcher::~Fetcher() {
  // Inform the waiting requests if it has not done so.
  if (waiting_requests_.size())
    InformWaitingRequests();
}

void OAuth2TokenService::Fetcher::Start() {
  fetcher_.reset(new OAuth2AccessTokenFetcher(this, getter_));
  fetcher_->Start(GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                  GaiaUrls::GetInstance()->oauth2_chrome_client_secret(),
                  refresh_token_,
                  std::vector<std::string>(scopes_.begin(), scopes_.end()));
  retry_timer_.Stop();
}

void OAuth2TokenService::Fetcher::OnGetTokenSuccess(
    const std::string& access_token,
    const base::Time& expiration_date) {
  fetcher_.reset();

  // Fetch completes.
  error_ = GoogleServiceAuthError::AuthErrorNone();
  access_token_ = access_token;
  expiration_date_ = expiration_date;

  // Subclasses may override this method to skip caching in some cases, but
  // we still inform all waiting Consumers of a successful token fetch below.
  // This is intentional -- some consumers may need the token for cleanup
  // tasks. https://chromiumcodereview.appspot.com/11312124/
  oauth2_token_service_->RegisterCacheEntry(refresh_token_,
                                            scopes_,
                                            access_token_,
                                            expiration_date_);
  // Deregisters itself from the service to prevent more waiting requests to
  // be added when it calls back the waiting requests.
  oauth2_token_service_->OnFetchComplete(this);
  InformWaitingRequests();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void OAuth2TokenService::Fetcher::OnGetTokenFailure(
    const GoogleServiceAuthError& error) {
  fetcher_.reset();

  if (ShouldRetry(error) && retry_number_ < kMaxFetchRetryNum) {
    int64 backoff = ComputeExponentialBackOffMilliseconds(retry_number_);
    ++retry_number_;
    retry_timer_.Stop();
    retry_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(backoff),
                       this,
                       &OAuth2TokenService::Fetcher::Start);
    return;
  }

  // Fetch completes.
  error_ = error;

  // Deregisters itself from the service to prevent more waiting requests to be
  // added when it calls back the waiting requests.
  oauth2_token_service_->OnFetchComplete(this);
  InformWaitingRequests();
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

// static
bool OAuth2TokenService::Fetcher::ShouldRetry(
    const GoogleServiceAuthError& error) {
  GoogleServiceAuthError::State error_state = error.state();
  return error_state == GoogleServiceAuthError::CONNECTION_FAILED ||
         error_state == GoogleServiceAuthError::REQUEST_CANCELED ||
         error_state == GoogleServiceAuthError::SERVICE_UNAVAILABLE;
}

void OAuth2TokenService::Fetcher::InformWaitingRequests() {
  std::vector<base::WeakPtr<RequestImpl> >::const_iterator iter =
      waiting_requests_.begin();
  for (; iter != waiting_requests_.end(); ++iter) {
    base::WeakPtr<RequestImpl> waiting_request = *iter;
    if (waiting_request)
      waiting_request->InformConsumer(error_, access_token_, expiration_date_);
  }
  waiting_requests_.clear();
}

void OAuth2TokenService::Fetcher::AddWaitingRequest(
    base::WeakPtr<OAuth2TokenService::RequestImpl> waiting_request) {
  waiting_requests_.push_back(waiting_request);
}

const OAuth2TokenService::ScopeSet& OAuth2TokenService::Fetcher::GetScopeSet()
    const {
  return scopes_;
}

const std::string& OAuth2TokenService::Fetcher::GetRefreshToken() const {
  return refresh_token_;
}

OAuth2TokenService::Request::Request() {
}

OAuth2TokenService::Request::~Request() {
}

OAuth2TokenService::Consumer::Consumer() {
}

OAuth2TokenService::Consumer::~Consumer() {
}

OAuth2TokenService::OAuth2TokenService(net::URLRequestContextGetter* getter)
    : request_context_getter_(getter) {
}

OAuth2TokenService::~OAuth2TokenService() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  // Release all the pending fetchers.
  STLDeleteContainerPairSecondPointers(
      pending_fetchers_.begin(), pending_fetchers_.end());
}

bool OAuth2TokenService::RefreshTokenIsAvailable() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  return !GetRefreshToken().empty();
}

// static
void OAuth2TokenService::InformConsumer(
    base::WeakPtr<OAuth2TokenService::RequestImpl> request,
    const GoogleServiceAuthError& error,
    const std::string& access_token,
    const base::Time& expiration_date) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (request)
    request->InformConsumer(error, access_token, expiration_date);
}

scoped_ptr<OAuth2TokenService::Request> OAuth2TokenService::StartRequest(
    const OAuth2TokenService::ScopeSet& scopes,
    OAuth2TokenService::Consumer* consumer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  scoped_ptr<RequestImpl> request(new RequestImpl(consumer));

  std::string refresh_token = GetRefreshToken();
  if (refresh_token.empty()) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &OAuth2TokenService::InformConsumer,
        request->AsWeakPtr(),
        GoogleServiceAuthError(
            GoogleServiceAuthError::USER_NOT_SIGNED_UP),
        std::string(),
        base::Time()));
    return request.PassAs<Request>();
  }

  if (HasCacheEntry(scopes))
    return StartCacheLookupRequest(scopes, consumer);

  // Makes sure there is a pending fetcher for |scopes| and |refresh_token|.
  // Adds |request| to the waiting request list of this fetcher so |request|
  // will be called back when this fetcher finishes fetching.
  FetchParameters fetch_parameters = std::make_pair(refresh_token, scopes);
  std::map<FetchParameters, Fetcher*>::iterator iter =
      pending_fetchers_.find(fetch_parameters);
  if (iter != pending_fetchers_.end()) {
    iter->second->AddWaitingRequest(request->AsWeakPtr());
    return request.PassAs<Request>();
  }
  pending_fetchers_[fetch_parameters] = Fetcher::CreateAndStart(
      this, request_context_getter_, refresh_token, scopes,
      request->AsWeakPtr());
  return request.PassAs<Request>();
}

scoped_ptr<OAuth2TokenService::Request>
    OAuth2TokenService::StartCacheLookupRequest(
        const OAuth2TokenService::ScopeSet& scopes,
        OAuth2TokenService::Consumer* consumer) {
  CHECK(HasCacheEntry(scopes));
  const CacheEntry* cache_entry = GetCacheEntry(scopes);
  scoped_ptr<RequestImpl> request(new RequestImpl(consumer));
  MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
      &OAuth2TokenService::InformConsumer,
      request->AsWeakPtr(),
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      cache_entry->access_token,
      cache_entry->expiration_date));
  return request.PassAs<Request>();
}

void OAuth2TokenService::InvalidateToken(const ScopeSet& scopes,
                                         const std::string& invalid_token) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  RemoveCacheEntry(scopes, invalid_token);
}

void OAuth2TokenService::OnFetchComplete(Fetcher* fetcher) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  // Update the auth error state so auth errors are appropriately communicated
  // to the user.
  UpdateAuthError(fetcher->error());

  // Note |fetcher| is recorded in |pending_fetcher_| mapped to its refresh
  // token and scope set. This is guaranteed as follows; here a Fetcher is said
  // to be uncompleted if it has not finished calling back
  // OAuth2TokenService::OnFetchComplete().
  //
  // (1) All the live Fetchers are created by this service.
  //     This is because (1) all the live Fetchers are created by a live
  //     service, as all the fetchers created by a service are destructed in the
  //     service's dtor.
  //
  // (2) All the uncompleted Fetchers created by this service are recorded in
  //     |pending_fetchers_|.
  //     This is because (1) all the created Fetchers are added to
  //     |pending_fetchers_| (in method StartRequest()) and (2) method
  //     OnFetchComplete() is the only place where a Fetcher is erased from
  //     |pending_fetchers_|. Note no Fetcher is erased in method
  //     StartRequest().
  //
  // (3) Each of the Fetchers recorded in |pending_fetchers_| is mapped to its
  //     refresh token and ScopeSet. This is guaranteed by Fetcher creation in
  //     method StartRequest().
  //
  // When this method is called, |fetcher| is alive and uncompleted.
  // By (1), |fetcher| is created by this service.
  // Then by (2), |fetcher| is recorded in |pending_fetchers_|.
  // Then by (3), |fetcher_| is mapped to its refresh token and ScopeSet.
  std::map<FetchParameters, Fetcher*>::iterator iter =
    pending_fetchers_.find(std::make_pair(
        fetcher->GetRefreshToken(), fetcher->GetScopeSet()));
  DCHECK(iter != pending_fetchers_.end());
  DCHECK_EQ(fetcher, iter->second);
  pending_fetchers_.erase(iter);
}

bool OAuth2TokenService::HasCacheEntry(
    const OAuth2TokenService::ScopeSet& scopes) {
  const CacheEntry* cache_entry = GetCacheEntry(scopes);
  return cache_entry && cache_entry->access_token.length();
}

const OAuth2TokenService::CacheEntry* OAuth2TokenService::GetCacheEntry(
    const OAuth2TokenService::ScopeSet& scopes) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  TokenCache::iterator token_iterator = token_cache_.find(scopes);
  if (token_iterator == token_cache_.end())
    return NULL;
  if (token_iterator->second.expiration_date <= base::Time::Now()) {
    token_cache_.erase(token_iterator);
    return NULL;
  }
  return &token_iterator->second;
}

bool OAuth2TokenService::RemoveCacheEntry(
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& token_to_remove) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  TokenCache::iterator token_iterator = token_cache_.find(scopes);
  if (token_iterator == token_cache_.end() &&
      token_iterator->second.access_token == token_to_remove) {
    token_cache_.erase(token_iterator);
    return true;
  }
  return false;
}

void OAuth2TokenService::RegisterCacheEntry(
    const std::string& refresh_token,
    const OAuth2TokenService::ScopeSet& scopes,
    const std::string& access_token,
    const base::Time& expiration_date) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  CacheEntry& token = token_cache_[scopes];
  token.access_token = access_token;
  token.expiration_date = expiration_date;
}

void OAuth2TokenService::UpdateAuthError(const GoogleServiceAuthError& error) {
  // Default implementation does nothing.
}

void OAuth2TokenService::ClearCache() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  token_cache_.clear();
}

int OAuth2TokenService::cache_size_for_testing() const {
  return token_cache_.size();
}
