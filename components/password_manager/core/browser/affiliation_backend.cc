// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation_database.h"
#include "components/password_manager/core/browser/affiliation_fetch_throttler.h"
#include "components/password_manager/core/browser/affiliation_fetcher.h"
#include "components/password_manager/core/browser/facet_manager.h"
#include "net/url_request/url_request_context_getter.h"

namespace password_manager {

AffiliationBackend::AffiliationBackend(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    scoped_ptr<base::Clock> time_source,
    scoped_ptr<base::TickClock> time_tick_source)
    : request_context_getter_(request_context_getter),
      task_runner_(task_runner),
      clock_(time_source.Pass()),
      tick_clock_(time_tick_source.Pass()),
      weak_ptr_factory_(this) {
  DCHECK_LT(base::Time(), clock_->Now());
}

AffiliationBackend::~AffiliationBackend() {
}

void AffiliationBackend::Initialize(const base::FilePath& db_path) {
  thread_checker_.reset(new base::ThreadChecker);

  DCHECK(!throttler_);
  throttler_.reset(
      new AffiliationFetchThrottler(this, task_runner_, tick_clock_.get()));

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

  FacetManager* facet_manager = GetOrCreateFacetManager(facet_uri);
  DCHECK(facet_manager);
  facet_manager->GetAffiliations(cached_only, callback, callback_task_runner);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  FacetManager* facet_manager = GetOrCreateFacetManager(facet_uri);
  DCHECK(facet_manager);
  facet_manager->Prefetch(keep_fresh_until);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  FacetManager* facet_manager = facet_managers_.get(facet_uri);
  if (!facet_manager)
    return;
  facet_manager->CancelPrefetch(keep_fresh_until);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::TrimCache() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Implement this. crbug.com/437865.
  NOTIMPLEMENTED();
}

FacetManager* AffiliationBackend::GetOrCreateFacetManager(
    const FacetURI& facet_uri) {
  if (!facet_managers_.contains(facet_uri)) {
    scoped_ptr<FacetManager> new_manager(
        new FacetManager(facet_uri, this, clock_.get()));
    facet_managers_.add(facet_uri, new_manager.Pass());
  }
  return facet_managers_.get(facet_uri);
}

void AffiliationBackend::OnSendNotification(const FacetURI& facet_uri) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  FacetManager* facet_manager = facet_managers_.get(facet_uri);
  if (!facet_manager)
    return;
  facet_manager->NotifyAtRequestedTime();

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

bool AffiliationBackend::ReadAffiliationsFromDatabase(
    const FacetURI& facet_uri,
    AffiliatedFacetsWithUpdateTime* affiliations) {
  return cache_->GetAffiliationsForFacet(facet_uri, affiliations);
}

void AffiliationBackend::SignalNeedNetworkRequest() {
  throttler_->SignalNetworkRequestNeeded();
}

void AffiliationBackend::RequestNotificationAtTime(const FacetURI& facet_uri,
                                                   base::Time time) {
  // TODO(engedy): Avoid spamming the task runner; only ever schedule the first
  // callback. crbug.com/437865.
  task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&AffiliationBackend::OnSendNotification,
                            weak_ptr_factory_.GetWeakPtr(), facet_uri),
      time - clock_->Now());
}

void AffiliationBackend::OnFetchSucceeded(
    scoped_ptr<AffiliationFetcherDelegate::Result> result) {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  fetcher_.reset();
  throttler_->InformOfNetworkRequestComplete(true);

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
      throttler_->SignalNetworkRequestNeeded();
      return;
    }
  }
}

void AffiliationBackend::OnFetchFailed() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  fetcher_.reset();
  throttler_->InformOfNetworkRequestComplete(false);

  // Trigger a retry if a fetch is still needed.
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch()) {
      throttler_->SignalNetworkRequestNeeded();
      return;
    }
  }
}

void AffiliationBackend::OnMalformedResponse() {
  DCHECK(thread_checker_ && thread_checker_->CalledOnValidThread());

  // TODO(engedy): Potentially handle this case differently. crbug.com/437865.
  OnFetchFailed();
}

bool AffiliationBackend::OnCanSendNetworkRequest() {
  DCHECK(!fetcher_);
  std::vector<FacetURI> requested_facet_uris;
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch())
      requested_facet_uris.push_back(facet_manager_pair.first);
  }

  // In case a request is no longer needed, return false to indicate this.
  if (requested_facet_uris.empty())
    return false;

  fetcher_.reset(AffiliationFetcher::Create(request_context_getter_.get(),
                                            requested_facet_uris, this));
  fetcher_->StartRequest();
  return true;
}

void AffiliationBackend::SetThrottlerForTesting(
    scoped_ptr<AffiliationFetchThrottler> throttler) {
  throttler_ = throttler.Pass();
}

}  // namespace password_manager
