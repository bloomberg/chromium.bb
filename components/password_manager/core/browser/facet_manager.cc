// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/facet_manager.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/task_runner.h"
#include "components/password_manager/core/browser/facet_manager_host.h"

namespace password_manager {

namespace {

// The duration after which cached affiliation data is considered stale and will
// not be used to serve requests any longer.
const int kCacheLifetimeInHours = 24;

}  // namespace

// Encapsulates the details of a pending GetAffiliations() request.
struct FacetManager::RequestInfo {
  AffiliationService::ResultCallback callback;
  scoped_refptr<base::TaskRunner> callback_task_runner;
};

FacetManager::FacetManager(FacetManagerHost* host, const FacetURI& facet_uri)
    : backend_(host),
      facet_uri_(facet_uri),
      last_update_time_(backend_->ReadLastUpdateTimeFromDatabase(facet_uri)) {
}

FacetManager::~FacetManager() {
  // The manager will only be destroyed while there are pending requests if the
  // entire backend is going. Call failure on pending requests in this case.
  for (const auto& request_info : pending_requests_)
    ServeRequestWithFailure(request_info);
}

void FacetManager::GetAffiliations(
    bool cached_only,
    const AffiliationService::ResultCallback& callback,
    const scoped_refptr<base::TaskRunner>& callback_task_runner) {
  RequestInfo request_info;
  request_info.callback = callback;
  request_info.callback_task_runner = callback_task_runner;
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

void FacetManager::OnFetchSucceeded(
    const AffiliatedFacetsWithUpdateTime& affiliation) {
  last_update_time_ = affiliation.last_update_time;
  DCHECK(IsCachedDataFresh()) << facet_uri_;
  for (const auto& request_info : pending_requests_)
    ServeRequestWithSuccess(request_info, affiliation.facets);
  pending_requests_.clear();
}

bool FacetManager::CanBeDiscarded() const {
  return pending_requests_.empty();
}

bool FacetManager::DoesRequireFetch() const {
  return !pending_requests_.empty() && !IsCachedDataFresh();
}

base::Time FacetManager::GetCacheExpirationTime() const {
  if (last_update_time_.is_null())
    return base::Time();
  return last_update_time_ + base::TimeDelta::FromHours(kCacheLifetimeInHours);
}

bool FacetManager::IsCachedDataFresh() const {
  return backend_->GetCurrentTime() < GetCacheExpirationTime();
}

// static
void FacetManager::ServeRequestWithSuccess(
    const RequestInfo& request_info,
    const AffiliatedFacets& affiliation) {
  request_info.callback_task_runner->PostTask(
      FROM_HERE, base::Bind(request_info.callback, affiliation, true));
}

// static
void FacetManager::ServeRequestWithFailure(const RequestInfo& request_info) {
  request_info.callback_task_runner->PostTask(
      FROM_HERE, base::Bind(request_info.callback, AffiliatedFacets(), false));
}

}  // namespace password_manager
