// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_GPU_DRIVER_INFO_MANAGER_ANDROID_H_
#define CHROME_BROWSER_GPU_GPU_DRIVER_INFO_MANAGER_ANDROID_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/gpu_data_manager_observer.h"

class PrefRegistrySimple;

// Class for managing the GPU driver information stored in profile.
// On Android, collecting GPU driver information is very expensive as it needs
// to create a temporary context. Caching the information in profile saves a lot
// of start up cost.
class GpuDriverInfoManager : public content::GpuDataManagerObserver {
 public:
  GpuDriverInfoManager();
  ~GpuDriverInfoManager() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  static std::unique_ptr<GpuDriverInfoManager> Create();

  void Initialize();

  // content::GpuDataManagerObserver
  void OnGpuInfoUpdate() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuDriverInfoManager);
};

#endif  // CHROME_BROWSER_GPU_GPU_DRIVER_INFO_MANAGER_ANDROID_H_
