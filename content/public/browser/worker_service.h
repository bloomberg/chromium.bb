// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_WORKER_SERVICE_H_
#pragma once

#include "content/common/content_export.h"

namespace content {

class WorkerServiceObserver;

// A singleton for managing HTML5 shared web workers. These are run in a
// separate process, since multiple renderer processes can be talking to a
// single shared worker.
class WorkerService {
 public:
  virtual ~WorkerService() {}

  // Returns the WorkerService singleton.
  CONTENT_EXPORT static WorkerService* GetInstance();

  virtual void AddObserver(WorkerServiceObserver* observer) = 0;
  virtual void RemoveObserver(WorkerServiceObserver* observer) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLUGIN_SERVICE_H_
