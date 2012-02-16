// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_OBSERVER_H_
#pragma once

#include "content/common/content_export.h"

namespace content {

// Observers can register themselves via GpuDataManager::AddObserver, and
// can un-register with GpuDataManager::RemoveObserver.
class GpuDataManagerObserver {
 public:
  // Called for any observers whenever there is a GPU info update.
  virtual void OnGpuInfoUpdate() = 0;

 protected:
  virtual ~GpuDataManagerObserver() {}
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_OBSERVER_H_
