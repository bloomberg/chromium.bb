// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_GPU_PROFILE_CACHE_H_
#define CHROME_BROWSER_GPU_GPU_PROFILE_CACHE_H_

class PrefRegistrySimple;

// Class for caching and initializing GPU data by storing them in local state.
class GpuProfileCache {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  static GpuProfileCache* Create();

  virtual ~GpuProfileCache() {}

  // Get cached gpu data in local state and send them to GpuDataManager.
  virtual void Initialize() = 0;
};

#endif  // CHROME_BROWSER_GPU_GPU_PROFILE_CACHE_H_

