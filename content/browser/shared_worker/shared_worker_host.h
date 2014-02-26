// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_

#include <vector>

#include "base/memory/scoped_ptr.h"

namespace content {
class SharedWorkerMessageFilter;
class SharedWorkerInstance;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel.
class SharedWorkerHost {
 public:
  explicit SharedWorkerHost(SharedWorkerInstance* instance);
  ~SharedWorkerHost();

  // Starts the SharedWorker in the renderer process which is associated with
  // |filter|.
  void Init(SharedWorkerMessageFilter* filter);

  SharedWorkerInstance* instance() { return instance_.get(); }
  int worker_route_id() const { return worker_route_id_; }

 private:
  scoped_ptr<SharedWorkerInstance> instance_;
  int worker_route_id_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
