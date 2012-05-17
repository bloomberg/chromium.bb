// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
#pragma once

#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/gpu_info.h"

class CommandLine;

class CONTENT_EXPORT GpuDataManagerImpl
    : public NON_EXPORTED_BASE(content::GpuDataManager) {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManagerImpl* GetInstance();

  // GpuDataManager implementation.
  virtual content::GpuFeatureType GetGpuFeatureType() OVERRIDE;
  virtual void SetGpuFeatureType(content::GpuFeatureType feature_type) OVERRIDE;
  virtual content::GPUInfo GetGPUInfo() const OVERRIDE;
  virtual bool GpuAccessAllowed() OVERRIDE;
  virtual void RequestCompleteGpuInfoIfNeeded() OVERRIDE;
  virtual bool IsCompleteGPUInfoAvailable() const OVERRIDE;
  virtual bool ShouldUseSoftwareRendering() OVERRIDE;
  virtual void RegisterSwiftShaderPath(const FilePath& path) OVERRIDE;
  virtual const base::ListValue& GetLogMessages() const OVERRIDE;
  virtual void AddObserver(content::GpuDataManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      content::GpuDataManagerObserver* observer) OVERRIDE;

  // Only update if the current GPUInfo is not finalized.
  void UpdateGpuInfo(const content::GPUInfo& gpu_info);

  void AddLogMessage(Value* msg);

  // Inserting disable-feature switches into renderer process command-line
  // in correspondence to preliminary gpu feature flags.
  void AppendRendererCommandLine(CommandLine* command_line);

  // Inserting switches into gpu process command-line: kUseGL,
  // kDisableGLMultisampling.
  void AppendGpuCommandLine(CommandLine* command_line);

  // This gets called when switching GPU might have happened.
  void HandleGpuSwitch();

  // Force the current card to be blacklisted (usually due to GPU process
  // crashes).
  void BlacklistCard();

 private:
  typedef ObserverListThreadSafe<content::GpuDataManagerObserver>
      GpuDataManagerObserverList;

  friend struct DefaultSingletonTraits<GpuDataManagerImpl>;

  GpuDataManagerImpl();
  virtual ~GpuDataManagerImpl();

  void Initialize();

  // If flags hasn't been set and GPUInfo is available, run through blacklist
  // and compute the flags.
  void UpdateGpuFeatureType(content::GpuFeatureType embedder_feature_type);

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  // If use-gl switch is osmesa or any, return true.
  bool UseGLIsOSMesaOrAny();

  // Try to switch to software rendering, if possible and necessary.
  void EnableSoftwareRenderingIfNecessary();

  bool complete_gpu_info_already_requested_;
  bool complete_gpu_info_available_;

  content::GpuFeatureType gpu_feature_type_;
  content::GpuFeatureType preliminary_gpu_feature_type_;

  content::GPUInfo gpu_info_;
  mutable base::Lock gpu_info_lock_;

  // Observers.
  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  ListValue log_messages_;
  bool software_rendering_;

  FilePath swiftshader_path_;

  // Current card force-blacklisted due to GPU crashes.
  bool card_blacklisted_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManagerImpl);
};

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_IMPL_H_
