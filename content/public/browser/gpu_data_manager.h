// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_

#include <list>
#include <string>

#include "base/callback_forward.h"
#include "base/process/process.h"
#include "content/common/content_export.h"

class GURL;

namespace gpu {
struct GPUInfo;
struct VideoMemoryUsageStats;
}

namespace content {

class GpuDataManagerObserver;

// This class is fully thread-safe.
class GpuDataManager {
 public:
  // Getter for the singleton.
  CONTENT_EXPORT static GpuDataManager* GetInstance();

  // This is only called by extensions testing.
  virtual void BlacklistWebGLForTesting() = 0;

  virtual bool IsFeatureBlacklisted(int feature) const = 0;
  virtual bool IsFeatureEnabled(int feature) const = 0;
  virtual bool IsWebGLEnabled() const = 0;
  virtual bool IsWebGL2Enabled() const = 0;

  virtual gpu::GPUInfo GetGPUInfo() const = 0;

  // This indicator might change because we could collect more GPU info or
  // because the GPU blacklist could be updated.
  // If this returns false, any further GPU access, including launching GPU
  // process, establish GPU channel, and GPU info collection, should be
  // blocked.
  // Can be called on any thread.
  // If |reason| is not nullptr and GPU access is blocked, upon return, |reason|
  // contains a description of the reason why GPU access is blocked.
  virtual bool GpuAccessAllowed(std::string* reason) const = 0;

  // Requests complete GPU info if it has not already been requested
  virtual void RequestCompleteGpuInfoIfNeeded() = 0;

  // Check if basic and context GPU info have been collected.
  virtual bool IsEssentialGpuInfoAvailable() const = 0;

  // On Windows, besides basic and context GPU info, it also checks if
  // DxDiagnostics have been collected.
  // On other platforms, it's the same as IsEsentialGpuInfoAvailable().
  virtual bool IsCompleteGpuInfoAvailable() const = 0;

  // Requests that the GPU process report its current video memory usage stats.
  virtual void RequestVideoMemoryUsageStatsUpdate(
      const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
          callback) const = 0;

  // Returns true if SwiftShader should be used.
  virtual bool ShouldUseSwiftShader() const = 0;

  // Registers/unregister |observer|.
  virtual void AddObserver(GpuDataManagerObserver* observer) = 0;
  virtual void RemoveObserver(GpuDataManagerObserver* observer) = 0;

  // Allows a given domain previously blocked from accessing 3D APIs
  // to access them again.
  virtual void UnblockDomainFrom3DAPIs(const GURL& url) = 0;

  // Set GL strings. This triggers a re-calculation of GPU blacklist
  // decision.
  virtual void SetGLStrings(const std::string& gl_vendor,
                            const std::string& gl_renderer,
                            const std::string& gl_version) = 0;

  // Obtain collected GL strings.
  virtual void GetGLStrings(std::string* gl_vendor,
                            std::string* gl_renderer,
                            std::string* gl_version) = 0;

  // Turn off all hardware acceleration.
  virtual void DisableHardwareAcceleration() = 0;

  // Whether a GPU is in use (as opposed to a software renderer).
  virtual bool HardwareAccelerationEnabled() const = 0;

  // Whether the browser compositor can be used.
  virtual bool CanUseGpuBrowserCompositor() const = 0;

  // Extensions that are currently disabled.
  virtual void GetDisabledExtensions(
      std::string* disabled_extensions) const = 0;

  // Sets the initial GPU information. This should happen before calculating
  // the backlists decision and applying commandline switches.
  virtual void SetGpuInfo(const gpu::GPUInfo& gpu_info) = 0;

 protected:
  virtual ~GpuDataManager() {}
};

};  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_DATA_MANAGER_H_
