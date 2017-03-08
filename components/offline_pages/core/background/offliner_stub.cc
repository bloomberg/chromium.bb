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

  completion_callback_ = completion_callback;
  progress_callback_ = progress_callback;

  // Post the callback on the run loop.
  if (enable_callback_) {
    const int64_t arbitrary_size = 153LL;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(progress_callback_, request, arbitrary_size));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(completion_callback_, request,
                              Offliner::RequestStatus::SAVED));
  }
  return true;
}

void OfflinerStub::Cancel(const CancelCallback& callback) {
  cancel_called_ = true;
  callback.Run(0LL);
}

bool OfflinerStub::HandleTimeout(const SavePageRequest& request) {
  if (snapshot_on_last_retry_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(completion_callback_, request,
                              Offliner::RequestStatus::SAVED));
    return true;
  }
  return false;
}

}  // namespace offline_pages
