// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "gpu/config/gpu_blacklist.h"

namespace base {
class CommandLine;
}

namespace gpu {
struct GpuPreferences;
struct VideoMemoryUsageStats;
}

namespace content {

class CONTENT_EXPORT GpuDataManagerImplPrivate {
 public:
  static GpuDataManagerImplPrivate* Create(GpuDataManagerImpl* owner);

  void InitializeForTesting(const gpu::GpuControlListData& gpu_blacklist_data,
                            const gpu::GPUInfo& gpu_info);
  gpu::GPUInfo GetGPUInfo() const;
  bool GpuAccessAllowed(std::string* reason) const;
  void RequestCompleteGpuInfoIfNeeded();
  bool IsEssentialGpuInfoAvailable() const;
  bool IsCompleteGpuInfoAvailable() const;
  bool IsGpuFeatureInfoAvailable() const;
  gpu::GpuFeatureStatus GetFeatureStatus(gpu::GpuFeatureType feature) const;
  void RequestVideoMemoryUsageStatsUpdate(
      const base::Callback<void(const gpu::VideoMemoryUsageStats& stats)>&
          callback) const;
  void AddObserver(GpuDataManagerObserver* observer);
  void RemoveObserver(GpuDataManagerObserver* observer);
  void UnblockDomainFrom3DAPIs(const GURL& url);
  void SetGLStrings(const std::string& gl_vendor,
                    const std::string& gl_renderer,
                    const std::string& gl_version);
  void DisableHardwareAcceleration();
  bool HardwareAccelerationEnabled() const;
  void DisableSwiftShader();
  void SetGpuInfo(const gpu::GPUInfo& gpu_info);

  void Initialize();

  void UpdateGpuInfo(const gpu::GPUInfo& gpu_info);
  void UpdateGpuFeatureInfo(const gpu::GpuFeatureInfo& gpu_feature_info);
  gpu::GpuFeatureInfo GetGpuFeatureInfo() const;

  void AppendRendererCommandLine(base::CommandLine* command_line) const;

  void AppendGpuCommandLine(base::CommandLine* command_line) const;

  void UpdateGpuPreferences(gpu::GpuPreferences* gpu_preferences) const;

  void GetBlacklistReasons(base::ListValue* reasons) const;

  std::vector<std::string> GetDriverBugWorkarounds() const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  void ProcessCrashed(base::TerminationStatus exit_code);

  std::unique_ptr<base::ListValue> GetLogMessages() const;

  void HandleGpuSwitch();

  void GetDisabledExtensions(std::string* disabled_extensions) const;

  void BlockDomainFrom3DAPIs(
      const GURL& url, GpuDataManagerImpl::DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& top_origin_url,
                        int render_process_id,
                        int render_frame_id,
                        ThreeDAPIType requester);

  void DisableDomainBlockingFor3DAPIsForTesting();

  void Notify3DAPIBlocked(const GURL& top_origin_url,
                          int render_process_id,
                          int render_frame_id,
                          ThreeDAPIType requester);

  bool UpdateActiveGpu(uint32_t vendor_id, uint32_t device_id);

  void OnGpuProcessInitFailure();

  virtual ~GpuDataManagerImplPrivate();

 private:
  friend class GpuDataManagerImplPrivateTest;

  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           GpuInfoUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           BlockAllDomainsFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockGuiltyDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockDomainOfUnknownGuiltFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockOtherDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplPrivateTest,
                           UnblockThisDomainFrom3DAPIs);

  struct DomainBlockEntry {
    GpuDataManagerImpl::DomainGuilt last_guilt;
  };

  typedef std::map<std::string, DomainBlockEntry> DomainBlockMap;

  typedef base::ObserverListThreadSafe<GpuDataManagerObserver>
      GpuDataManagerObserverList;

  struct LogMessage {
    int level;
    std::string header;
    std::string message;

    LogMessage(int _level,
               const std::string& _header,
               const std::string& _message)
        : level(_level),
          header(_header),
          message(_message) { }
  };

  explicit GpuDataManagerImplPrivate(GpuDataManagerImpl* owner);

  void InitializeImpl(const gpu::GpuControlListData& gpu_blacklist_data,
                      const gpu::GPUInfo& gpu_info);

  void RunPostInitTasks();

  void UpdateGpuInfoHelper();

  void UpdateBlacklistedFeatures(const std::set<int>& features);

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  // Helper to extract the domain from a given URL.
  std::string GetDomainFromURL(const GURL& url) const;

  // Implementation functions for blocking of 3D graphics APIs, used
  // for unit testing.
  void BlockDomainFrom3DAPIsAtTime(const GURL& url,
                                   GpuDataManagerImpl::DomainGuilt guilt,
                                   base::Time at_time);
  GpuDataManagerImpl::DomainBlockStatus Are3DAPIsBlockedAtTime(
      const GURL& url, base::Time at_time) const;
  int64_t GetBlockAllDomainsDurationInMs() const;

  bool complete_gpu_info_already_requested_;

  std::set<int> blacklisted_features_;

  // Eventually |blacklisted_features_| should be folded in to this.
  gpu::GpuFeatureInfo gpu_feature_info_;

  gpu::GPUInfo gpu_info_;

  std::unique_ptr<gpu::GpuBlacklist> gpu_blacklist_;

  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  std::vector<LogMessage> log_messages_;

  // Current card force-disabled due to GPU crashes, or disabled through
  // the --disable-gpu commandline switch.
  bool card_disabled_;

  // SwiftShader force-disabled due to GPU crashes using SwiftShader.
  bool swiftshader_disabled_;

  // We disable histogram stuff in testing, especially in unit tests because
  // they cause random failures.
  bool update_histograms_;

  DomainBlockMap blocked_domains_;
  mutable std::list<base::Time> timestamps_of_gpu_resets_;
  bool domain_blocking_enabled_;

  GpuDataManagerImpl* owner_;

  bool gpu_process_accessible_;

  // True if Initialize() has been completed.
  bool is_initialized_;

  // True if all future Initialize calls should be ignored.
  bool finalized_;

  // True if --single-process or --in-process-gpu is passed in.
  bool in_process_gpu_;

  // If one tries to call a member before initialization then it is defered
  // until Initialize() is completed.
  std::vector<base::Closure> post_init_tasks_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImplPrivate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_PRIVATE_H_
