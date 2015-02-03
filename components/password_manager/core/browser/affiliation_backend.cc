// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation_database.h"
#include "components/password_manager/core/browser/affiliation_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace password_manager {
namespace {

// The duration after which cached affiliation data is considered stale and will
// not be used to serve requests any longer.
const int kCacheLifetimeInHours = 24;

// RequestInfo ----------------------------------------------------------------

// Encapsulates the details of a pending GetAffiliations() request.
struct RequestInfo {
  AffiliationService::ResultCallback callback;
  scoped_refptr<base::TaskRunner> callback_task_runner;
};

}  // namespace

// AffiliationBackend::FacetManager -------------------------------------------

// Manages and performs ongoing tasks concerning a single facet.
class AffiliationBackend::FacetManager {
 public:
  // The |backend| must outlive this object.
  FacetManager(AffiliationBackend* backend, const FacetURI& facet_uri);
  ~FacetManager();

  // Called when |affiliation| information regarding this facet has just been
  // fetched from the Affiliation API.
  void OnFetchSucceeded(const AffiliatedFacetsWithUpdateTime& affiliation);

  // Returns whether this instance has becomes redundant, that is, it has no
  // more meaningful state than a newly created instance would have.
  bool CanBeDiscarded() const;

  // Returns whether or not affiliation information relating to this facet needs
  // to be fetched right now.
  bool DoesRequireFetch() const;

  // Facet-specific implementations for methods in AffiliationService of the
  // same name. See documentation in affiliation_service.h for details:
  void GetAffiliations(bool cached_only, const RequestInfo& request_info);

 private:
  // Returns the time when cached data for this facet will expire. The data is
  // already considered expired at the returned microsecond.
  base::Time GetCacheExpirationTime() const;

  // Returns whether or not the cache has fresh data for this facet.
  bool IsCachedDataFresh() const;

  // Posts the callback of the request described by |request_info| with success.
  static void ServeRequestWithSuccess(const RequestInfo& request_info,
                                      const AffiliatedFacets& affiliation);

  // Posts the callback of the request described by |request_info| with failure.
  static void ServeRequestWithFailure(const RequestInfo& request_info);

  AffiliationBackend* backend_;
  FacetURI facet_uri_;

  // The last time affiliation information was fetched for this facet, i.e. the
  // freshness of the data in the cache. If there is no corresponding data in
  // the database, this will contain the NULL time. Otherwise, the update time
  // in the database should match this value; it is stored to reduce disk I/O.
  base::Time last_update_time_;

  // Contains information about the GetAffiliations() requests that are waiting
  // for the result of looking up this facet.
  std::vector<RequestInfo> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(FacetManager);
};

AffiliationBackend::FacetManager::FacetManager(AffiliationBackend* backend,
                                               const FacetURI& facet_uri)
    : backend_(backend),
      facet_uri_(facet_uri),
      last_update_time_(backend_->ReadLastUpdateTimeFromDatabase(facet_uri)) {
}

AffiliationBackend::FacetManager::~FacetManager() {
  // The manager will only be destroyed while there are pending requests if the
  // entire backend is going. Call failure on pending requests in this case.
  for (const auto& request_info : pending_requests_)
    ServeRequestWithFailure(request_info);
}

void AffiliationBackend::FacetManager::GetAffiliations(
    bool cached_only,
    const RequestInfo& request_info) {
  if (IsCachedDataFresh()) {
    AffiliatedFacetsWithUpdateTime affiliation;
    if (!backend_->ReadAffiliationsFromDatabase(facet_uri_, &affiliation)) {
      // TODO(engedy): Implement this. crbug.com/437865.
      NOTIMPLEMENTED();
    }
    DCHECK_EQ(affiliation.last_update_time, last_update_time_) << facet_uri_;
    ServeRequestWithSuccess(request_info, affiliation.facets);
  } else if (cached_only) {
    ServeRequestWithFailure(request_info);
  } else {
    pending_requests_.push_back(request_info);
    backend_->SignalNeedNetworkRequest();
  }
}

void AffiliationBackend::FacetManager::OnFetchSucceeded(
    const AffiliatedFacetsWithUpdateTime& affiliation) {
  last_update_time_ = affiliation.last_update_time;
  DCHECK(IsCachedDataFresh()) << facet_uri_;
  for (const auto& request_info : pending_requests_)
    ServeRequestWithSuccess(request_info, affiliation.facets);
  pending_requests_.clear();
}

bool AffiliationBackend::FacetManager::CanBeDiscarded() const {
  return pending_requests_.empty();
}

bool AffiliationBackend::FacetManager::DoesRequireFetch() const {
  return !pending_requests_.empty() && !IsCachedDataFresh();
}

base::Time AffiliationBackend::FacetManager::GetCacheExpirationTime() const {
  if (last_update_time_.is_null())
    return base::Time();
  return last_update_time_ + base::TimeDelta::FromHours(kCacheLifetimeInHours);
}

bool AffiliationBackend::FacetManager::IsCachedDataFresh() const {
  return backend_->GetCurrentTime() < GetCacheExpirationTime();
}

// static
void AffiliationBackend::FacetManager::ServeRequestWithSuccess(
    const RequestInfo& request_info,
    const AffiliatedFacets& affiliation) {
  request_info.callback_task_runner->PostTask(
      FROM_HERE, base::Bind(request_info.callback, affiliation, true));
}

// static
void AffiliationBackend::FacetManager::ServeRequestWithFailure(
    const RequestInfo& request_info) {
  request_info.callback_task_runner->PostTask(
      FROM_HERE, base::Bind(request_info.callback, AffiliatedFacets(), false));
}

// AffiliationBackend ---------------------------------------------------------

AffiliationBackend::AffiliationBackend(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    scoped_ptr<base::Clock> time_source)
    : request_context_getter_(request_context_getter),
      clock_(time_source.Pass()),
      weak_ptr_factory_(this) {
  DCHECK_LT(base::Time(), clock_->Now());
}

AffiliationBackend::~AffiliationBackend() {
}

void AffiliationBackend::Initialize(const base::FilePath& db_path) {
  thread_checker_.reset(new base::ThreadChecker);
  cache_.reset(new AffiliationDatabase());
  if (!cache_->Init(db_path)) {
    // TODO(engedy): Implement this. crbug.com/437865.
    NOTIMPLEMENTED();
  }
}

void AffiliationBackend::GetAffiliations(
    const FacetURI& facet_uri,
    bool cached_only,
    const AffiliationService::ResultCallback& callback,
    const scoped_refptr<base::TaskRunner>& callback_task_runner) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  if (!facet_managers_.contains(facet_uri)) {
    scoped_ptr<FacetManager> new_manager(new FacetManager(this, facet_uri));
    facet_managers_.add(facet_uri, new_manager.Pass());
  }

  FacetManager* facet_manager = facet_managers_.get(facet_uri);
  DCHECK(facet_manager);
  RequestInfo request_info;
  request_info.callback = callback;
  request_info.callback_task_runner = callback_task_runner;
  facet_manager->GetAffiliations(cached_only, request_info);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::TrimCache() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::SendNetworkRequest() {
  DCHECK(!fetcher_);

  std::vector<FacetURI> requested_facet_uris;
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch())
      requested_facet_uris.push_back(facet_manager_pair.first);
  }
  DCHECK(!requested_facet_uris.empty());
  fetcher_.reset(AffiliationFetcher::Create(request_context_getter_.get(),
                                            requested_facet_uris, this));
  fetcher_->StartRequest();
}

base::Time AffiliationBackend::GetCurrentTime() {
  return clock_->Now();
}

base::Time AffiliationBackend::ReadLastUpdateTimeFromDatabase(
    const FacetURI& facet_uri) {
  AffiliatedFacetsWithUpdateTime affiliation;
  return ReadAffiliationsFromDatabase(facet_uri, &affiliation)
             ? affiliation.last_update_time
             : base::Time();
}

bool AffiliationBackend::ReadAffiliationsFromDatabase(
    const FacetURI& facet_uri,
    AffiliatedFacetsWithUpdateTime* affiliations) {
  return cache_->GetAffiliationsForFacet(facet_uri, affiliations);
}

void AffiliationBackend::SignalNeedNetworkRequest() {
  // TODO(engedy): Add more sophisticated throttling logic. crbug.com/437865.
  if (fetcher_)
    return;
  SendNetworkRequest();
}

void AffiliationBackend::OnFetchSucceeded(
    scoped_ptr<AffiliationFetcherDelegate::Result> result) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());
  fetcher_.reset();

  for (const AffiliatedFacets& affiliated_facets : *result) {
    AffiliatedFacetsWithUpdateTime affiliation;
    affiliation.facets = affiliated_facets;
    affiliation.last_update_time = clock_->Now();

    std::vector<AffiliatedFacetsWithUpdateTime> obsoleted_affiliations;
    cache_->StoreAndRemoveConflicting(affiliation, &obsoleted_affiliations);

    // Cached data in contradiction with newly stored data automatically gets
    // removed from the DB, and will be stored into |obsoleted_affiliations|.
    // Let facet managers know if data is removed from under them.
    // TODO(engedy): Implement this. crbug.com/437865.
    if (!obsoleted_affiliations.empty())
      NOTIMPLEMENTED();

    for (const auto& facet_uri : affiliated_facets) {
      if (!facet_managers_.contains(facet_uri))
        continue;
      FacetManager* facet_manager = facet_managers_.get(facet_uri);
      facet_manager->OnFetchSucceeded(affiliation);
      if (facet_manager->CanBeDiscarded())
        facet_managers_.erase(facet_uri);
    }
  }

  // A subsequent fetch may be needed if any additional GetAffiliations()
  // requests came in while the current fetch was in flight.
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch()) {
      SendNetworkRequest();
      return;
    }
  }
}

void AffiliationBackend::OnFetchFailed() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

void AffiliationBackend::OnMalformedResponse() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

}  // namespace password_manager
