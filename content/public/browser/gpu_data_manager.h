// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/common/gpu_feature_type.h"

class FilePath;

namespace base {
class ListValue;
}

namespace content {

class GpuDataManagerObserver;
struct GPUInfo;

// This class is fully thread-safe.
class GpuDataManager {
 public:
  // Getter for the singleton.
  CONTENT_EXPORT static GpuDataManager* GetInstance();

  // This collects preliminary GPU info, load GpuBlacklist, and compute the
  // preliminary blacklisted features; it should only be called at browser
  // startup time in UI thread before the IO restriction is turned on.
  virtual void Initialize(const std::string& browser_version_string,
                          const std::string& gpu_blacklist_json) = 0;
  // The same as above, except that the GPUInfo is provided.
  // For testing only.
  virtual void Initialize(const std::string& browser_version_string,
                          const std::string& gpu_blacklist_json,
                          const content::GPUInfo& gpu_info) = 0;

  virtual std::string GetBlacklistVersion() const = 0;

  virtual GpuFeatureType GetBlacklistedFeatures() const = 0;

  virtual GpuSwitchingOption GetGpuSwitchingOption() const = 0;

  // Returns the reasons for the latest run of blacklisting decisions.
  // For the structure of returned value, see documentation for
  // GpuBlacklist::GetBlacklistedReasons().
  // Caller is responsible to release the returned value.
  virtual base::ListValue* GetBlacklistReasons() const = 0;

  virtual GPUInfo GetGPUInfo() const = 0;

  // This indicator might change because we could collect more GPU info or
  // because the GPU blacklist could be updated.
  // If this returns false, any further GPU access, including launching GPU
  // process, establish GPU channel, and GPU info collection, should be
  // blocked.
  // Can be called on any thread.
  virtual bool GpuAccessAllowed() const = 0;

  // Requests complete GPUinfo if it has not already been requested
  virtual void RequestCompleteGpuInfoIfNeeded() = 0;

  virtual bool IsCompleteGpuInfoAvailable() const = 0;

  // Requests that the GPU process report its current video memory usage stats,
  // which can be retrieved via the GPU data manager's on-update function.
  virtual void RequestVideoMemoryUsageStatsUpdate() const = 0;

  // Returns true if the software rendering should currently be used.
  virtual bool ShouldUseSoftwareRendering() const = 0;

  // Register a path to the SwiftShader software renderer.
  virtual void RegisterSwiftShaderPath(const FilePath& path) = 0;

  virtual void AddLogMessage(
      int level, const std::string& header, const std::string& message) = 0;

  // Returns a new copy of the ListValue.  Caller is responsible to release
  // the returned value.
  virtual base::ListValue* GetLogMessages() const = 0;

  // Registers/unregister |observer|.
  virtual void AddObserver(GpuDataManagerObserver* observer) = 0;
  virtual void RemoveObserver(GpuDataManagerObserver* observer) = 0;

 protected:
  virtual ~GpuDataManager() {}
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
