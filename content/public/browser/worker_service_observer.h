// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_OBSERVER_H_
#pragma once

#include "base/process.h"
#include "base/string16.h"

class GURL;

namespace content {

class WorkerServiceObserver {
 public:
  virtual ~WorkerServiceObserver() {}

  virtual void WorkerCreated(const GURL& url,
                             const string16& name,
                             int process_id,
                             int route_id) {}
  virtual void WorkerDestroyed(int process_id, int route_id) {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_OBSERVER_H_
