// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"

namespace content {

// WorkerThread-related methods only exposed within the content module.
class WorkerThreadImpl {
 public:
  static void DidStartCurrentWorkerThread();
  static void WillStopCurrentWorkerThread();

 private:
  DISALLOW_COPY_AND_ASSIGN(WorkerThreadImpl);
};

}  // namespace content
