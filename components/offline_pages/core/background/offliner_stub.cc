// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/offliner_stub.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace offline_pages {

OfflinerStub::OfflinerStub()
    : disable_loading_(false),
      enable_callback_(false),
      cancel_called_(false),
      snapshot_on_last_retry_(false) {}

OfflinerStub::~OfflinerStub() {}

bool OfflinerStub::LoadAndSave(const SavePageRequest& request,
                               const CompletionCallback& completion_callback,
                               const ProgressCallback& progress_callback) {
  if (disable_loading_)
    return false;

  pending_request_.reset(new SavePageRequest(request));
  completion_callback_ = completion_callback;

  // Post the callback on the run loop.
  if (enable_callback_) {
    const int64_t arbitrary_size = 153LL;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(progress_callback, request, arbitrary_size));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(completion_callback, *pending_request_.get(),
                              Offliner::RequestStatus::SAVED));
  }
  return true;
}

bool OfflinerStub::Cancel(const CancelCallback& callback) {
  cancel_called_ = true;
  if (!pending_request_)
    return false;

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, *pending_request_.get()));
  pending_request_.reset();
  return true;
}

void OfflinerStub::TerminateLoadIfInProgress() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(completion_callback_, *pending_request_.get(),
                            Offliner::RequestStatus::FOREGROUND_CANCELED));
  pending_request_.reset();
}

bool OfflinerStub::HandleTimeout(int64_t request_id) {
  if (snapshot_on_last_retry_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(completion_callback_, *pending_request_.get(),
                              Offliner::RequestStatus::SAVED));
    pending_request_.reset();
    return true;
  }
  return false;
}

}  // namespace offline_pages
