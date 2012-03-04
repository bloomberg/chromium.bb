// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
#pragma once

#include "content/common/content_export.h"
#include "content/public/common/gpu_feature_type.h"

class FilePath;

namespace base {
class ListValue;
}

namespace content {

class GpuDataManagerObserver;
struct GPUInfo;

// This class lives on the UI thread. Only methods that explicitly state that
// they can be called on other threads are thread-safe.
class GpuDataManager {
 public:
  // Getter for the singleton.
  CONTENT_EXPORT static GpuDataManager* GetInstance();

  // Can be called on any thread.
  virtual GpuFeatureType GetGpuFeatureType() = 0;

  // Gives the new feature flags.  This is always called on the UI thread.
  virtual void SetGpuFeatureType(GpuFeatureType feature_type) = 0;

  virtual GPUInfo GetGPUInfo() const = 0;

  // This indicator might change because we could collect more GPU info or
  // because the GPU blacklist could be updated.
  // If this returns false, any further GPU access, including launching GPU
  // process, establish GPU channel, and GPU info collection, should be
  // blocked.
  // Can be called on any thread.
  virtual bool GpuAccessAllowed() = 0;

  // Requests complete GPUinfo if it has not already been requested
  virtual void RequestCompleteGpuInfoIfNeeded() = 0;

  virtual bool IsCompleteGPUInfoAvailable() const = 0;

  // Returns true if the software rendering should currently be used.
  virtual bool ShouldUseSoftwareRendering() = 0;

  // Register a path to the SwiftShader software renderer.
  virtual void RegisterSwiftShaderPath(const FilePath& path) = 0;

  virtual const base::ListValue& GetLogMessages() const = 0;

  // Registers/unregister |observer|.
  virtual void AddObserver(GpuDataManagerObserver* observer) = 0;
  virtual void RemoveObserver(GpuDataManagerObserver* observer) = 0;

 protected:
  virtual ~GpuDataManager() {}
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
