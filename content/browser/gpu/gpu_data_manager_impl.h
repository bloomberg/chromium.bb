// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/gpu_info.h"
#include "content/public/common/gpu_memory_stats.h"

class CommandLine;

class CONTENT_EXPORT GpuDataManagerImpl
    : public NON_EXPORTED_BASE(content::GpuDataManager) {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManagerImpl* GetInstance();

  // GpuDataManager implementation.
  virtual void Initialize(
      const std::string& browser_version_string,
      const std::string& gpu_blacklist_json) OVERRIDE;
  virtual void Initialize(
      const std::string& browser_version_string,
      const std::string& gpu_blacklist_json,
      const content::GPUInfo& gpu_info) OVERRIDE;
  virtual content::GpuFeatureType GetBlacklistedFeatures() const OVERRIDE;
  virtual content::GpuSwitchingOption GetGpuSwitchingOption() const OVERRIDE;
  virtual base::ListValue* GetBlacklistReasons() const OVERRIDE;
  virtual std::string GetBlacklistVersion() const OVERRIDE;
  virtual content::GPUInfo GetGPUInfo() const OVERRIDE;
  virtual bool GpuAccessAllowed() const OVERRIDE;
  virtual void RequestCompleteGpuInfoIfNeeded() OVERRIDE;
  virtual bool IsCompleteGpuInfoAvailable() const OVERRIDE;
  virtual void RequestVideoMemoryUsageStatsUpdate() const OVERRIDE;
  virtual bool ShouldUseSoftwareRendering() const OVERRIDE;
  virtual void RegisterSwiftShaderPath(const FilePath& path) OVERRIDE;
  virtual void AddLogMessage(int level, const std::string& header,
                             const std::string& message) OVERRIDE;
  virtual base::ListValue* GetLogMessages() const OVERRIDE;
  virtual void AddObserver(content::GpuDataManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      content::GpuDataManagerObserver* observer) OVERRIDE;

  // Only update if the current GPUInfo is not finalized.  If blacklist is
  // loaded, run through blacklist and update blacklisted features.
  void UpdateGpuInfo(const content::GPUInfo& gpu_info);

  void UpdateVideoMemoryUsageStats(
      const content::GPUVideoMemoryUsageStats& video_memory_usage_stats);

  // Insert disable-feature switches corresponding to preliminary gpu feature
  // flags into the renderer process command line.
  void AppendRendererCommandLine(CommandLine* command_line) const;

  // Insert switches into gpu process command line: kUseGL,
  // kDisableGLMultisampling.
  void AppendGpuCommandLine(CommandLine* command_line) const;

  // Insert switches into plugin process command line:
  // kDisableCoreAnimationPlugins.
  void AppendPluginCommandLine(CommandLine* command_line) const;

  // Force the current card to be blacklisted (usually due to GPU process
  // crashes).
  void BlacklistCard();

#if defined(OS_WIN)
  // Is the GPU process using the accelerated surface to present, instead of
  // presenting by itself.
  bool IsUsingAcceleratedSurface() const;
#endif

 private:
  typedef ObserverListThreadSafe<content::GpuDataManagerObserver>
      GpuDataManagerObserverList;

  friend class GpuDataManagerImplTest;
  friend struct DefaultSingletonTraits<GpuDataManagerImpl>;

  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, GpuSideBlacklisting);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, GpuSideExceptions);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, BlacklistCard);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest, GpuInfoUpdate);
  FRIEND_TEST_ALL_PREFIXES(GpuDataManagerImplTest,
                           GPUVideoMemoryUsageStatsUpdate);

  GpuDataManagerImpl();
  virtual ~GpuDataManagerImpl();

  void UpdateBlacklistedFeatures(content::GpuFeatureType features);

  // This should only be called once at initialization time, when preliminary
  // gpu info is collected.
  void UpdatePreliminaryBlacklistedFeatures();

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  // Try to switch to software rendering, if possible and necessary.
  void EnableSoftwareRenderingIfNecessary();

  // Send UMA histograms about the disabled/enabled features.
  void UpdateStats();

  bool complete_gpu_info_already_requested_;

  content::GpuFeatureType gpu_feature_type_;
  content::GpuFeatureType preliminary_gpu_feature_type_;

  content::GpuSwitchingOption gpu_switching_;

  content::GPUInfo gpu_info_;
  mutable base::Lock gpu_info_lock_;

  scoped_ptr<GpuBlacklist> gpu_blacklist_;

  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  ListValue log_messages_;
  mutable base::Lock log_messages_lock_;

  bool software_rendering_;

  FilePath swiftshader_path_;

  // Current card force-blacklisted due to GPU crashes, or disabled through
  // the --disable-gpu commandline switch.
  bool card_blacklisted_;

  // We disable histogram stuff in testing, especially in unit tests because
  // they cause random failures.
  bool update_histograms_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImpl);
};

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
