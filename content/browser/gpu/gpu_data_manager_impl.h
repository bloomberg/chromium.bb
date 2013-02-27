// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_

#include <list>
#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "base/values.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/gpu_info.h"
#include "content/public/common/gpu_memory_stats.h"
#include "content/public/common/three_d_api_types.h"

class CommandLine;
class GURL;

namespace content {

class CONTENT_EXPORT GpuDataManagerImpl
    : public NON_EXPORTED_BASE(GpuDataManager) {
 public:
  // Indicates the guilt level of a domain which caused a GPU reset.
  // If a domain is 100% known to be guilty of resetting the GPU, then
  // it will generally not cause other domains' use of 3D APIs to be
  // blocked, unless system stability would be compromised.
  enum DomainGuilt {
    DOMAIN_GUILT_KNOWN,
    DOMAIN_GUILT_UNKNOWN
  };

  // Indicates the reason that access to a given client API (like
  // WebGL or Pepper 3D) was blocked or not. This state is distinct
  // from blacklisting of an entire feature.
  enum DomainBlockStatus {
    DOMAIN_BLOCK_STATUS_BLOCKED,
    DOMAIN_BLOCK_STATUS_ALL_DOMAINS_BLOCKED,
    DOMAIN_BLOCK_STATUS_NOT_BLOCKED
  };

  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManagerImpl* GetInstance();

  // GpuDataManager implementation.
  virtual void InitializeForTesting(
      const std::string& gpu_blacklist_json,
      const GPUInfo& gpu_info) OVERRIDE;
  virtual GpuFeatureType GetBlacklistedFeatures() const OVERRIDE;
  virtual GPUInfo GetGPUInfo() const OVERRIDE;
  virtual void GetGpuProcessHandles(
      const GetGpuProcessHandlesCallback& callback) const OVERRIDE;
  virtual bool GpuAccessAllowed() const OVERRIDE;
  virtual void RequestCompleteGpuInfoIfNeeded() OVERRIDE;
  virtual bool IsCompleteGpuInfoAvailable() const OVERRIDE;
  virtual void RequestVideoMemoryUsageStatsUpdate() const OVERRIDE;
  virtual bool ShouldUseSoftwareRendering() const OVERRIDE;
  virtual void RegisterSwiftShaderPath(const base::FilePath& path) OVERRIDE;
  virtual void AddObserver(GpuDataManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(GpuDataManagerObserver* observer) OVERRIDE;
  virtual void SetWindowCount(uint32 count) OVERRIDE;
  virtual uint32 GetWindowCount() const OVERRIDE;
  virtual void UnblockDomainFrom3DAPIs(const GURL& url) OVERRIDE;
  virtual void DisableGpuWatchdog() OVERRIDE;
  virtual void SetGLStrings(const std::string& gl_vendor,
                            const std::string& gl_renderer,
                            const std::string& gl_version) OVERRIDE;
  virtual void GetGLStrings(std::string* gl_vendor,
                            std::string* gl_renderer,
                            std::string* gl_version) OVERRIDE;

  // This collects preliminary GPU info, load GpuBlacklist, and compute the
  // preliminary blacklisted features; it should only be called at browser
  // startup time in UI thread before the IO restriction is turned on.
  void Initialize();

  // Only update if the current GPUInfo is not finalized.  If blacklist is
  // loaded, run through blacklist and update blacklisted features.
  void UpdateGpuInfo(const GPUInfo& gpu_info);

  void UpdateVideoMemoryUsageStats(
      const GPUVideoMemoryUsageStats& video_memory_usage_stats);

  // Insert disable-feature switches corresponding to preliminary gpu feature
  // flags into the renderer process command line.
  void AppendRendererCommandLine(CommandLine* command_line) const;

  // Insert switches into gpu process command line: kUseGL,
  // kDisableGLMultisampling.
  void AppendGpuCommandLine(CommandLine* command_line) const;

  // Insert switches into plugin process command line:
  // kDisableCoreAnimationPlugins.
  void AppendPluginCommandLine(CommandLine* command_line) const;

  GpuSwitchingOption GetGpuSwitchingOption() const;

  // Force the current card to be blacklisted (usually due to GPU process
  // crashes).
  void BlacklistCard();

  std::string GetBlacklistVersion() const;

  // Returns the reasons for the latest run of blacklisting decisions.
  // For the structure of returned value, see documentation for
  // GpuBlacklist::GetBlacklistedReasons().
  // Caller is responsible to release the returned value.
  base::ListValue* GetBlacklistReasons() const;

  void AddLogMessage(int level,
                     const std::string& header,
                     const std::string& message);

  // Returns a new copy of the ListValue.  Caller is responsible to release
  // the returned value.
  base::ListValue* GetLogMessages() const;

  // Called when switching gpu.
  void HandleGpuSwitch();

#if defined(OS_WIN)
  // Is the GPU process using the accelerated surface to present, instead of
  // presenting by itself.
  bool IsUsingAcceleratedSurface() const;
#endif

  // Maintenance of domains requiring explicit user permission before
  // using client-facing 3D APIs (WebGL, Pepper 3D), either because
  // the domain has caused the GPU to reset, or because too many GPU
  // resets have been observed globally recently, and system stability
  // might be compromised.
  //
  // The given URL may be a partial URL (including at least the host)
  // or a full URL to a page.
  //
  // Note that the unblocking API must be part of the content API
  // because it is called from Chrome side code.
  void BlockDomainFrom3DAPIs(const GURL& url, DomainGuilt guilt);
  bool Are3DAPIsBlocked(const GURL& url,
                        int render_process_id,
                        int render_view_id,
                        ThreeDAPIType requester);

  // Disables domain blocking for 3D APIs. For use only in tests.
  void DisableDomainBlockingFor3DAPIsForTesting();

  // Called when GPU process initialization failed.
  void OnGpuProcessInitFailure();

 private:
  struct DomainBlockEntry {
    DomainGuilt last_guilt;
  };

  typedef std::map<std::string, DomainBlockEntry> DomainBlockMap;

  typedef ObserverListThreadSafe<GpuDataManagerObserver>
      GpuDataManagerObserverList;

  friend class GpuDataManagerImplTest;
  friend struct DefaultSingletonTraits<GpuDataManagerImpl>;

  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, GpuSideBlacklisting);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, GpuSideExceptions);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, BlacklistCard);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, SoftwareRendering);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, SoftwareRendering2);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, GpuInfoUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           NoGpuInfoUpdateWithSoftwareRendering);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           GPUVideoMemoryUsageStatsUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           BlockAllDomainsFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           UnblockGuiltyDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           UnblockDomainOfUnknownGuiltFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           UnblockOtherDomainFrom3DAPIs);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           UnblockThisDomainFrom3DAPIs);

  GpuDataManagerImpl();
  virtual ~GpuDataManagerImpl();

  void InitializeImpl(const std::string& gpu_blacklist_json,
                      const GPUInfo& gpu_info);

  void UpdateBlacklistedFeatures(GpuFeatureType features);

  // This should only be called once at initialization time, when preliminary
  // gpu info is collected.
  void UpdatePreliminaryBlacklistedFeatures();

  // Update the GPU switching status.
  // This should only be called once at initialization time.
  void UpdateGpuSwitchingManager(const GPUInfo& gpu_info);

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  // Try to switch to software rendering, if possible and necessary.
  void EnableSoftwareRenderingIfNecessary();

  // Helper to extract the domain from a given URL.
  std::string GetDomainFromURL(const GURL& url) const;

  // Implementation functions for blocking of 3D graphics APIs, used
  // for unit testing.
  void BlockDomainFrom3DAPIsAtTime(
      const GURL& url, DomainGuilt guilt, base::Time at_time);
  DomainBlockStatus Are3DAPIsBlockedAtTime(
      const GURL& url, base::Time at_time) const;
  int64 GetBlockAllDomainsDurationInMs() const;

  void Notify3DAPIBlocked(const GURL& url,
                          int render_process_id,
                          int render_view_id,
                          ThreeDAPIType requester);

  bool complete_gpu_info_already_requested_;

  GpuFeatureType blacklisted_features_;
  GpuFeatureType preliminary_blacklisted_features_;

  GpuSwitchingOption gpu_switching_;

  GPUInfo gpu_info_;
  mutable base::Lock gpu_info_lock_;

  scoped_ptr<GpuBlacklist> gpu_blacklist_;

  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  ListValue log_messages_;
  mutable base::Lock log_messages_lock_;

  bool software_rendering_;

  base::FilePath swiftshader_path_;

  // Current card force-blacklisted due to GPU crashes, or disabled through
  // the --disable-gpu commandline switch.
  bool card_blacklisted_;

  // We disable histogram stuff in testing, especially in unit tests because
  // they cause random failures.
  bool update_histograms_;

  // Number of currently open windows, to be used in gpu memory allocation.
  int window_count_;

  DomainBlockMap blocked_domains_;
  mutable std::list<base::Time> timestamps_of_gpu_resets_;
  bool domain_blocking_enabled_;

  bool gpu_process_accessible_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
