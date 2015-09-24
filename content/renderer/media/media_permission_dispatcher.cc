// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_permission_dispatcher.h"

#include "base/logging.h"

namespace content {

MediaPermissionDispatcher::MediaPermissionDispatcher() : next_request_id_(0) {}

MediaPermissionDispatcher::~MediaPermissionDispatcher() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Fire all pending callbacks with |false|.
  for (auto& request : requests_)
    request.second.Run(false);

  requests_.clear();
}

uint32_t MediaPermissionDispatcher::RegisterCallback(
    const PermissionStatusCB& permission_status_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  uint32_t request_id = next_request_id_++;
  DCHECK(!requests_.count(request_id));
  requests_[request_id] = permission_status_cb;
  return request_id;
}

void MediaPermissionDispatcher::DeliverResult(uint32_t request_id,
                                              bool granted) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(2) << __FUNCTION__ << ": (" << request_id << ", " << granted << ")";

  RequestMap::iterator iter = requests_.find(request_id);
  if (iter == requests_.end()) {
    DVLOG(2) << "Request not found.";
    return;
  }

  PermissionStatusCB permission_status_cb = iter->second;
  requests_.erase(iter);

  permission_status_cb.Run(granted);
}

}  // namespace content
