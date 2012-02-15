// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
#pragma once

#include <set>
#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/values.h"
#include "content/browser/gpu/gpu_performance_stats.h"
#include "content/common/content_export.h"
#include "content/public/common/gpu_feature_type.h"
#include "content/public/common/gpu_info.h"

class CommandLine;

class CONTENT_EXPORT GpuDataManager {
 public:
  // Observers can register themselves via GpuDataManager::AddObserver, and
  // can un-register with GpuDataManager::RemoveObserver.
  class Observer {
   public:
    // Called for any observers whenever there is a GPU info update.
    virtual void OnGpuInfoUpdate() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManager* GetInstance();

  // Requests complete GPUinfo if it has not already been requested
  void RequestCompleteGpuInfoIfNeeded();

  // Only update if the current GPUInfo is not finalized.
  void UpdateGpuInfo(const content::GPUInfo& gpu_info);

  const content::GPUInfo& gpu_info() const;

  bool complete_gpu_info_available() const {
    return complete_gpu_info_available_;
  }

  GpuPerformanceStats GetPerformanceStats() const;

  void AddLogMessage(Value* msg);

  const ListValue& log_messages() const;

  // Can be called on any thread.
  content::GpuFeatureType GetGpuFeatureType();

  // This indicator might change because we could collect more GPU info or
  // because the GPU blacklist could be updated.
  // If this returns false, any further GPU access, including launching GPU
  // process, establish GPU channel, and GPU info collection, should be
  // blocked.
  // Can be called on any thread.
  bool GpuAccessAllowed();

  // Registers |observer|. The thread on which this is called is the thread
  // on which |observer| will be called back with notifications. |observer|
  // must not be NULL.
  void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications. This must be called
  // on the same thread on which AddObserver() was called. |observer|
  // must not be NULL.
  void RemoveObserver(Observer* observer);

  // Inserting disable-feature switches into renderer process command-line
  // in correspondance to preliminary gpu feature flags.
  void AppendRendererCommandLine(CommandLine* command_line);

  // Inserting switches into gpu process command-line: kUseGL,
  // kDisableGLMultisampling.
  void AppendGpuCommandLine(CommandLine* command_line);

  // Gives the new feature flags.  This is always called on the UI thread.
  void SetGpuFeatureType(content::GpuFeatureType feature_type);

  // This gets called when switching GPU might have happened.
  void HandleGpuSwitch();

  // Returns true if the software rendering should currently be used.
  bool software_rendering();

  // Register a path to the SwiftShader software renderer.
  void RegisterSwiftShaderPath(FilePath path);

  // Force the current card to be blacklisted (usually due to GPU process
  // crashes).
  void BlacklistCard();

 private:
  typedef ObserverListThreadSafe<GpuDataManager::Observer>
      GpuDataManagerObserverList;

  friend struct DefaultSingletonTraits<GpuDataManager>;

  GpuDataManager();
  virtual ~GpuDataManager();

  void Initialize();

  // If flags hasn't been set and GPUInfo is available, run through blacklist
  // and compute the flags.
  void UpdateGpuFeatureType(content::GpuFeatureType embedder_feature_type);

  // Notify all observers whenever there is a GPU info update.
  void NotifyGpuInfoUpdate();

  // If use-gl switch is osmesa or any, return true.
  bool UseGLIsOSMesaOrAny();

  // Merges the second GPUInfo object with the first.
  // If it's the same GPU, i.e., device id and vendor id are the same, then
  // copy over the fields that are not set yet and ignore the rest.
  // If it's a different GPU, then reset and copy over everything.
  // Return true if something changes that may affect blacklisting; currently
  // they are device_id, vendor_id, driver_vendor, driver_version, driver_date,
  // and gl_renderer.
  static bool Merge(content::GPUInfo* object, const content::GPUInfo& other);

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

  DISALLOW_COPY_AND_ASSIGN(GpuDataManager);
};

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
