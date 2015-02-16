// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FACET_MANAGER_HOST_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FACET_MANAGER_HOST_H_

#include "base/macros.h"

namespace password_manager {

// Interface through which FacetManagers can access shared functionality
// provided by the AffiliationBackend.
class FacetManagerHost {
 public:
  virtual ~FacetManagerHost() {}

  // Gets the current time. The returned time will always be strictly greater
  // than the NULL time.
  virtual base::Time GetCurrentTime() = 0;

  // Reads and returns the last update time of the equivalence class containing
  // |facet_uri| from the database, or, if no such equivalence class is stored,
  // returns the NULL time.
  virtual base::Time ReadLastUpdateTimeFromDatabase(
      const FacetURI& facet_uri) = 0;

  // Reads the equivalence class containing |facet_uri| from the database and
  // returns true if found; returns false otherwise.
  virtual bool ReadAffiliationsFromDatabase(
      const FacetURI& facet_uri,
      AffiliatedFacetsWithUpdateTime* affiliations) = 0;

  // Signals the fetching logic that affiliation information for a facet needs
  // to be fetched immediately.
  virtual void SignalNeedNetworkRequest() = 0;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FACET_MANAGER_HOST_H_
