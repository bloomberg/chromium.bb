// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_ORIGIN_OPERATION_QUEUE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_ORIGIN_OPERATION_QUEUE_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "url/gurl.h"

namespace sync_file_system {

struct OriginOperation {
  enum Type {
    UNKNOWN,
    REGISTERING,
    ENABLING,
    DISABLING,
    UNINSTALLING
  };

  GURL origin;
  Type type;

  OriginOperation();
  OriginOperation(const GURL& origin, Type type);
  ~OriginOperation();
};

class OriginOperationQueue {
 public:
  OriginOperationQueue();
  ~OriginOperationQueue();

  void Push(const GURL& origin, OriginOperation::Type type);
  OriginOperation Pop();
  bool HasPendingOperation(const GURL& origin) const;

  size_t size() const { return queue_.size(); }
  bool empty() const { return queue_.empty(); }

 private:
  std::deque<OriginOperation> queue_;
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_ORIGIN_OPERATION_QUEUE_H_
