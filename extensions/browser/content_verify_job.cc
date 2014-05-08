// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/content_verify_job.h"

#include <algorithm>

#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {

namespace {

ContentVerifyJob::TestDelegate* g_test_delegate = NULL;

}  // namespace

ContentVerifyJob::ContentVerifyJob(const std::string& extension_id,
                                   const FailureCallback& failure_callback)
    : extension_id_(extension_id), failure_callback_(failure_callback) {
  // It's ok for this object to be constructed on a different thread from where
  // it's used.
  thread_checker_.DetachFromThread();
}

ContentVerifyJob::~ContentVerifyJob() {
}

void ContentVerifyJob::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void ContentVerifyJob::BytesRead(int count, const char* data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (g_test_delegate) {
    FailureReason reason =
        g_test_delegate->BytesRead(extension_id_, count, data);
    if (reason != NONE)
      return DispatchFailureCallback(reason);
  }
}

void ContentVerifyJob::DoneReading() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (g_test_delegate) {
    FailureReason reason = g_test_delegate->DoneReading(extension_id_);
    if (reason != NONE) {
      DispatchFailureCallback(reason);
      return;
    }
  }
}

// static
void ContentVerifyJob::SetDelegateForTests(TestDelegate* delegate) {
  g_test_delegate = delegate;
}

void ContentVerifyJob::DispatchFailureCallback(FailureReason reason) {
  if (!failure_callback_.is_null()) {
    failure_callback_.Run(reason);
    failure_callback_.Reset();
  }
}

}  // namespace extensions
