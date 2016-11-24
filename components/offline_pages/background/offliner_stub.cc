// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/offliner_stub.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

OfflinerStub::OfflinerStub()
    : disable_loading_(false), enable_callback_(false), cancel_called_(false) {}

OfflinerStub::~OfflinerStub() {}

bool OfflinerStub::LoadAndSave(const SavePageRequest& request,
                               const CompletionCallback& callback) {
  if (disable_loading_)
    return false;

  callback_ = callback;

  // Post the callback on the run loop.
  if (enable_callback_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback, request, Offliner::RequestStatus::SAVED));
  }
  return true;
}

void OfflinerStub::Cancel() {
  cancel_called_ = true;
}

}  // namespace offline_pages
