// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_test_util.h"

BrowsingDataRemoverCompletionObserver::BrowsingDataRemoverCompletionObserver(
    BrowsingDataRemover* remover)
    : message_loop_runner_(new content::MessageLoopRunner) {
  remover->AddObserver(this);
}

BrowsingDataRemoverCompletionObserver::
    ~BrowsingDataRemoverCompletionObserver() {}

void BrowsingDataRemoverCompletionObserver::BlockUntilCompletion() {
  message_loop_runner_->Run();
}

void BrowsingDataRemoverCompletionObserver::OnBrowsingDataRemoverDone() {
  message_loop_runner_->Quit();
}

BrowsingDataRemoverCompletionInhibitor::BrowsingDataRemoverCompletionInhibitor()
    : message_loop_runner_(new content::MessageLoopRunner) {
  BrowsingDataRemover::set_completion_inhibitor_for_testing(this);
}

BrowsingDataRemoverCompletionInhibitor::
    ~BrowsingDataRemoverCompletionInhibitor() {
  BrowsingDataRemover::set_completion_inhibitor_for_testing(NULL);
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
    BrowsingDataRemover* remover,
    const base::Closure& continue_to_completion) {
  DCHECK(continue_to_completion_callback_.is_null());
  continue_to_completion_callback_ = continue_to_completion;
  message_loop_runner_->Quit();
}
