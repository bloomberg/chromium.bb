// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_GL_STRING_MANAGER_H_
#define CHROME_BROWSER_GPU_GL_STRING_MANAGER_H_

#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/gpu_data_manager_observer.h"

class PrefRegistrySimple;

class GLStringManager : public content::GpuDataManagerObserver {
 public:
  static void RegisterPrefs(PrefRegistrySimple* registry);

  GLStringManager();
  virtual ~GLStringManager();

  // Get cached GL strings in local state and send them to GpuDataManager.
  void Initialize();

  // content::GpuDataManagerObserver
  virtual void OnGpuInfoUpdate() OVERRIDE;

 private:
  std::string gl_vendor_;
  std::string gl_renderer_;
  std::string gl_version_;

  DISALLOW_COPY_AND_ASSIGN(GLStringManager);
};

#endif  // CHROME_BROWSER_GPU_GL_STRING_MANAGER_H_

