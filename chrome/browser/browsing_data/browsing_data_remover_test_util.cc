// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"

BrowsingDataRemoverCompletionObserver::BrowsingDataRemoverCompletionObserver(
    BrowsingDataRemover* remover)
    : message_loop_runner_(new content::MessageLoopRunner), observer_(this) {
  observer_.Add(remover);
}

BrowsingDataRemoverCompletionObserver::
    ~BrowsingDataRemoverCompletionObserver() {}

void BrowsingDataRemoverCompletionObserver::BlockUntilCompletion() {
  message_loop_runner_->Run();
}

void BrowsingDataRemoverCompletionObserver::OnBrowsingDataRemoverDone() {
  observer_.RemoveAll();
  message_loop_runner_->Quit();
}

BrowsingDataRemoverCompletionInhibitor::BrowsingDataRemoverCompletionInhibitor(
    BrowsingDataRemover* remover)
    : remover_(remover), message_loop_runner_(new content::MessageLoopRunner) {
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
  message_loop_runner_->Run();
  message_loop_runner_ = new content::MessageLoopRunner;
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
  message_loop_runner_->Quit();
}
