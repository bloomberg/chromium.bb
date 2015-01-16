// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation_backend.h"

#include "base/task_runner.h"

namespace password_manager {

AffiliationBackend::AffiliationBackend() {
}

AffiliationBackend::~AffiliationBackend() {
}

void AffiliationBackend::Initialize() {
  NOTIMPLEMENTED();
}

void AffiliationBackend::GetAffiliations(
    const FacetURI& facet_uri,
    bool cached_only,
    const AffiliationService::ResultCallback& callback,
    scoped_refptr<base::TaskRunner> callback_task_runner) {
  NOTIMPLEMENTED();
}

void AffiliationBackend::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  NOTIMPLEMENTED();
}

void AffiliationBackend::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  NOTIMPLEMENTED();
}

void AffiliationBackend::TrimCache() {
  NOTIMPLEMENTED();
}

}  // namespace password_manager
