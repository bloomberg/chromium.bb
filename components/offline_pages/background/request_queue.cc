// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/background/request_queue.h"

#include "components/offline_pages/background/save_page_request.h"

namespace offline_pages {

void RequestQueue::GetRequests(const GetRequestsCallback& callback) {}

void RequestQueue::AddRequest(const SavePageRequest& request,
                              const AddRequestCallback& callback) {}

void RequestQueue::UpdateRequest(const SavePageRequest& request,
                                 const UpdateRequestCallback& callback) {}

void RequestQueue::RemoveRequest(int64_t request_id,
                                 const UpdateRequestCallback& callback) {}

void RequestQueue::PurgeRequests(const PurgeRequestsCallback& callback) {}

}  // namespace offline_pages
