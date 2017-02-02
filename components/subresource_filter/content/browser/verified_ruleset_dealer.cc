// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/logging.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

namespace subresource_filter {

// VerifiedRulesetDealer and its Handle. ---------------------------------------

VerifiedRulesetDealer::VerifiedRulesetDealer() = default;
VerifiedRulesetDealer::~VerifiedRulesetDealer() = default;

void VerifiedRulesetDealer::SetRulesetFile(base::File ruleset_file) {
  RulesetDealer::SetRulesetFile(std::move(ruleset_file));
  status_ = RulesetVerificationStatus::NOT_VERIFIED;
}

scoped_refptr<const MemoryMappedRuleset> VerifiedRulesetDealer::GetRuleset() {
  DCHECK(CalledOnValidThread());

  // TODO(pkalinnikov): Record verification status to a histogram.
  switch (status_) {
    case RulesetVerificationStatus::NOT_VERIFIED: {
      auto ruleset = RulesetDealer::GetRuleset();
      if (ruleset &&
          IndexedRulesetMatcher::Verify(ruleset->data(), ruleset->length())) {
        status_ = RulesetVerificationStatus::INTACT;
        return ruleset;
      }
      status_ = RulesetVerificationStatus::CORRUPT;
      return nullptr;
    }
    case RulesetVerificationStatus::INTACT: {
      auto ruleset = RulesetDealer::GetRuleset();
      DCHECK(ruleset);
      return ruleset;
    }
    case RulesetVerificationStatus::CORRUPT:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

VerifiedRulesetDealer::Handle::Handle(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner.get()),
      dealer_(new VerifiedRulesetDealer,
              base::OnTaskRunnerDeleter(std::move(task_runner))) {}

VerifiedRulesetDealer::Handle::~Handle() = default;

void VerifiedRulesetDealer::Handle::GetDealerAsync(
    base::Callback<void(VerifiedRulesetDealer*)> callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // NOTE: Properties of the sequenced |task_runner| guarantee that the
  // |callback| will always be provided with a valid pointer, because the
  // corresponding task will be posted *before* a task to delete the pointer
  // upon destruction of |this| Handler.
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(std::move(callback), dealer_.get()));
}

void VerifiedRulesetDealer::Handle::SetRulesetFile(base::File file) {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&VerifiedRulesetDealer::SetRulesetFile,
                 base::Unretained(dealer_.get()), base::Passed(&file)));
}

// VerifiedRuleset and its Handle. ---------------------------------------------

VerifiedRuleset::VerifiedRuleset() {
  thread_checker_.DetachFromThread();
}

VerifiedRuleset::~VerifiedRuleset() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void VerifiedRuleset::Initialize(VerifiedRulesetDealer* dealer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(dealer);
  ruleset_ = dealer->GetRuleset();
}

VerifiedRuleset::Handle::Handle(VerifiedRulesetDealer::Handle* dealer_handle)
    : task_runner_(dealer_handle->task_runner()),
      ruleset_(new VerifiedRuleset, base::OnTaskRunnerDeleter(task_runner_)) {
  dealer_handle->GetDealerAsync(base::Bind(&VerifiedRuleset::Initialize,
                                           base::Unretained(ruleset_.get())));
}

VerifiedRuleset::Handle::~Handle() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void VerifiedRuleset::Handle::GetRulesetAsync(
    base::Callback<void(VerifiedRuleset*)> callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  task_runner_->PostTask(FROM_HERE, base::Bind(callback, ruleset_.get()));
}

}  // namespace subresource_filter
