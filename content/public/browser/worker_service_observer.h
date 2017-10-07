// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_OBSERVER_H_

#include <string>

class GURL;

namespace content {

// All the methods below are called on the UI thread.
class WorkerServiceObserver {
 public:
  virtual void WorkerCreated(const GURL& url,
                             const std::string& name,
                             int process_id,
                             int route_id) {}
  virtual void WorkerDestroyed(int process_id, int route_id) {}

 protected:
  virtual ~WorkerServiceObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_OBSERVER_H_
