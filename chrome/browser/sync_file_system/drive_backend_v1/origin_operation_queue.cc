// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/origin_operation_queue.h"

#include "base/logging.h"

namespace sync_file_system {

OriginOperation::OriginOperation() : type(UNKNOWN) {}
OriginOperation::OriginOperation(const GURL& origin, Type type)
    : origin(origin), type(type) {}
OriginOperation::~OriginOperation() {}

OriginOperationQueue::OriginOperationQueue() {}
OriginOperationQueue::~OriginOperationQueue() {}

void OriginOperationQueue::Push(const GURL& origin,
                                OriginOperation::Type type) {
  DCHECK_NE(OriginOperation::UNKNOWN, type);
  queue_.push_back(OriginOperation(origin, type));
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
    if (iter->origin == origin)
      return true;
  }

  return false;
}

}  // namespace sync_file_system
