// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FACET_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FACET_MANAGER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/affiliation_service.h"
#include "components/password_manager/core/browser/affiliation_utils.h"

namespace base {
class TaskRunner;
}  // namespace base

namespace password_manager {

class FacetManagerHost;

// Part of AffiliationBackend that encapsulates the state and logic required for
// handling GetAffiliations() requests in regard to a single facet.
//
// In contrast, the AffiliationBackend itself implements the FacetManagerHost
// interface to provide shared functionality needed by all FacetManagers.
class FacetManager {
 public:
  // The |backend| must outlive this object.
  FacetManager(FacetManagerHost* backend, const FacetURI& facet_uri);
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
  void GetAffiliations(
      bool cached_only,
      const AffiliationService::ResultCallback& callback,
      const scoped_refptr<base::TaskRunner>& callback_task_runner);

 private:
  struct RequestInfo;

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

  FacetManagerHost* backend_;
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

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FACET_MANAGER_H_
