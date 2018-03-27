// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browsing_data_remover_test_util.h"

#include "base/bind.h"
#include "base/task_scheduler/task_scheduler.h"

namespace content {

BrowsingDataRemoverCompletionObserver::BrowsingDataRemoverCompletionObserver(
    BrowsingDataRemover* remover)
    : observer_(this) {
  observer_.Add(remover);
}

BrowsingDataRemoverCompletionObserver::
    ~BrowsingDataRemoverCompletionObserver() {}

void BrowsingDataRemoverCompletionObserver::BlockUntilCompletion() {
  base::TaskScheduler::GetInstance()->FlushForTesting();
  run_loop_.Run();
}

void BrowsingDataRemoverCompletionObserver::OnBrowsingDataRemoverDone() {
  observer_.RemoveAll();
  run_loop_.QuitWhenIdle();
}

BrowsingDataRemoverCompletionInhibitor::BrowsingDataRemoverCompletionInhibitor(
    BrowsingDataRemover* remover)
    : remover_(remover), run_loop_(new base::RunLoop) {
  DCHECK(remover);
  remover_->SetWouldCompleteCallbackForTesting(
      base::Bind(&BrowsingDataRemoverCompletionInhibitor::
                     OnBrowsingDataRemoverWouldComplete,
                 base::Unretained(this)));
}

BrowsingDataRemoverCompletionInhibitor::
    ~BrowsingDataRemoverCompletionInhibitor() {
  Reset();
}

void BrowsingDataRemoverCompletionInhibitor::Reset() {
  if (!remover_)
    return;
  remover_->SetWouldCompleteCallbackForTesting(
      base::Callback<void(const base::Closure&)>());
  remover_ = nullptr;
}

void BrowsingDataRemoverCompletionInhibitor::BlockUntilNearCompletion() {
  base::TaskScheduler::GetInstance()->FlushForTesting();
  run_loop_->Run();
  run_loop_ = std::make_unique<base::RunLoop>();
}

void BrowsingDataRemoverCompletionInhibitor::ContinueToCompletion() {
  DCHECK(!continue_to_completion_callback_.is_null());
  continue_to_completion_callback_.Run();
  continue_to_completion_callback_.Reset();
}

void BrowsingDataRemoverCompletionInhibitor::OnBrowsingDataRemoverWouldComplete(
    const base::Closure& continue_to_completion) {
  DCHECK(continue_to_completion_callback_.is_null());
  continue_to_completion_callback_ = continue_to_completion;
  run_loop_->QuitWhenIdle();
}

}  // namespace content
