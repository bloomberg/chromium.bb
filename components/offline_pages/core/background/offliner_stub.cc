// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/background/offliner_stub.h"

#include <utility>

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
                               CompletionCallback completion_callback,
                               const ProgressCallback& progress_callback) {
  load_and_save_called_ = true;
  if (disable_loading_)
    return false;

  pending_request_.reset(new SavePageRequest(request));
  completion_callback_ = std::move(completion_callback);

  // Post the callback on the run loop.
  if (enable_callback_) {
    const int64_t arbitrary_size = 153LL;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(progress_callback, request, arbitrary_size));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback_), *pending_request_,
                       Offliner::RequestStatus::SAVED));
  }
  return true;
}

bool OfflinerStub::Cancel(CancelCallback callback) {
  cancel_called_ = true;
  if (!pending_request_)
    return false;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(std::move(callback), *pending_request_),
      cancel_delay_);
  pending_request_.reset();
  return true;
}

void OfflinerStub::TerminateLoadIfInProgress() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(completion_callback_), *pending_request_,
                     Offliner::RequestStatus::FOREGROUND_CANCELED));
  pending_request_.reset();
}

bool OfflinerStub::HandleTimeout(int64_t request_id) {
  if (snapshot_on_last_retry_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback_), *pending_request_,
                       Offliner::RequestStatus::SAVED));
    pending_request_.reset();
    return true;
  }
  return false;
}

}  // namespace offline_pages
