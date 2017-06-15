// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_

#include "url/gurl.h"

namespace content {

class ServiceWorkerContextObserver {
 public:
  // Called when a service worker has been registered with scope |pattern|.
  virtual void OnRegistrationStored(const GURL& pattern) {}

 protected:
  virtual ~ServiceWorkerContextObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
