// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/origin_operation_queue.h"

#include "base/logging.h"

namespace sync_file_system {

OriginOperation::OriginOperation() : type(UNKNOWN), aborted(true) {}
OriginOperation::OriginOperation(const GURL& origin, Type type)
    : origin(origin), type(type), aborted(false) {}
OriginOperation::~OriginOperation() {}

OriginOperationQueue::OriginOperationQueue() {}
OriginOperationQueue::~OriginOperationQueue() {}

void OriginOperationQueue::Push(const GURL& origin,
                                OriginOperation::Type type) {
  DCHECK_NE(OriginOperation::UNKNOWN, type);
  OriginOperation new_operation(origin, type);

  if (queue_.empty() || queue_.back().origin != origin) {
    queue_.push_back(new_operation);
    return;
  }

  if ((queue_.back().type == OriginOperation::DISABLING &&
           type == OriginOperation::ENABLING) ||
      (queue_.back().type == OriginOperation::ENABLING &&
           type == OriginOperation::DISABLING)) {
    queue_.back().aborted = true;
    new_operation.aborted = true;
  }

  queue_.push_back(new_operation);
}

OriginOperation OriginOperationQueue::Pop() {
  DCHECK(!queue_.empty());
  OriginOperation operation = queue_.front();
  queue_.pop_front();
  return operation;
}

bool OriginOperationQueue::HasPendingOperation(const GURL& origin) const {
  if (queue_.empty())
    return false;

  for (std::deque<OriginOperation>::const_iterator iter = queue_.begin();
       iter != queue_.end(); ++iter) {
    // If we have any un-aborted operation, return true.
    if (iter->origin == origin && !iter->aborted)
      return true;
  }

  return false;
}

}  // namespace sync_file_system
