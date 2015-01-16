// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_BACKEND_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_BACKEND_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/password_manager/core/browser/affiliation_service.h"

namespace base {
class Time;
class FilePath;
class TaskRunner;
}

namespace password_manager {

class FacetURI;

// Implements the bulk of AffiliationService; runs on a background thread.
class AffiliationBackend {
 public:
  AffiliationBackend();
  ~AffiliationBackend();

  void Initialize();

  // Implementations for functions of the same name in AffiliationService.
  void GetAffiliations(const FacetURI& facet_uri,
                       bool cached_only,
                       const AffiliationService::ResultCallback& callback,
                       scoped_refptr<base::TaskRunner> callback_task_runner);
  void Prefetch(const FacetURI& facet_uri, const base::Time& keep_fresh_until);
  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until);
  void TrimCache();

 private:
  DISALLOW_COPY_AND_ASSIGN(AffiliationBackend);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_BACKEND_H_
