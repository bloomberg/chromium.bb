// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_backend.h"
#include "net/url_request/url_request_context_getter.h"

namespace password_manager {

AffiliationService::AffiliationService(
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : backend_(nullptr),
      backend_task_runner_(backend_task_runner),
      weak_ptr_factory_(this) {}

AffiliationService::~AffiliationService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (backend_) {
    backend_task_runner_->DeleteSoon(FROM_HERE, backend_);
    backend_ = nullptr;
  }
}

void AffiliationService::Initialize(
    net::URLRequestContextGetter* request_context_getter,
    const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_);
  backend_ =
      new AffiliationBackend(request_context_getter, backend_task_runner_,
                             base::WrapUnique(new base::DefaultClock),
                             base::WrapUnique(new base::DefaultTickClock));

  std::unique_ptr<base::TickClock> tick_clock(new base::DefaultTickClock);
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AffiliationBackend::Initialize,
                            base::Unretained(backend_), db_path));
}

void AffiliationService::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    StrategyOnCacheMiss cache_miss_strategy,
    const ResultCallback& result_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::GetAffiliationsAndBranding,
                 base::Unretained(backend_), facet_uri, cache_miss_strategy,
                 result_callback, base::SequencedTaskRunnerHandle::Get()));
}

void AffiliationService::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::Prefetch, base::Unretained(backend_),
                 facet_uri, keep_fresh_until));
}

void AffiliationService::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AffiliationBackend::CancelPrefetch,
                 base::Unretained(backend_), facet_uri, keep_fresh_until));
}

void AffiliationService::TrimCacheForFacetURI(const FacetURI& facet_uri) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(backend_);
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AffiliationBackend::TrimCacheForFacetURI,
                            base::Unretained(backend_), facet_uri));
}

}  // namespace password_manager
