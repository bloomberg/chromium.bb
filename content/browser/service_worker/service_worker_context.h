// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_H_

#include "base/basictypes.h"

namespace content {

// Represents the per-BrowserContext ServiceWorker data.
class ServiceWorkerContext {
 public:
  // TODO(michaeln): This class is a place holder for content/public api
  // which will come later. Promote this class when we get there.

 protected:
  ServiceWorkerContext() {}
  virtual ~ServiceWorkerContext() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTEXT_H_
