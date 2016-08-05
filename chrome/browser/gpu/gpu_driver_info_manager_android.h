// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_GPU_DRIVER_INFO_MANAGER_ANDROID_H_
#define CHROME_BROWSER_GPU_GPU_DRIVER_INFO_MANAGER_ANDROID_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/gpu/gpu_profile_cache.h"
#include "content/public/browser/gpu_data_manager_observer.h"

// Class for managing the GPU driver information stored in profile.
// On Android, collecting GPU driver information is very expensive as it needs
// to create a temporary context. Caching the information in profile saves a lot
// of start up cost.
class GpuDriverInfoManager : public GpuProfileCache,
                             public content::GpuDataManagerObserver {
 public:
  GpuDriverInfoManager();
  ~GpuDriverInfoManager() override;

  // GpuProfileCache.
  void Initialize() override;

  // content::GpuDataManagerObserver
  void OnGpuInfoUpdate() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuDriverInfoManager);
};

#endif  // CHROME_BROWSER_GPU_GPU_DRIVER_INFO_MANAGER_ANDROID_H_
