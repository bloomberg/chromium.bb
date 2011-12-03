// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/task.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_feature_flags.h"
#include "content/public/common/gpu_info.h"

class CommandLine;
class GpuBlacklist;

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

  // Returns status of various GPU features. This is two parted:
  // {
  //    featureStatus: []
  //    problems: []
  // }
  //
  // Each entry in feature_status has:
  // {
  //    name:  "name of feature"
  //    status: "enabled" | "unavailable_software" | "unavailable_off" |
  //            "software" | "disabled_off" | "disabled_softare";
  // }
  //
  // The features reported are not 1:1 with GpuFeatureType. Rather, they are:
  //    '2d_canvas'
  //    '3d_css'
  //    'composting',
  //    'webgl',
  //    'multisampling'
  //
  // Each problems has:
  // {
  //    "description": "Your GPU is too old",
  //    "crBugs": [1234],
  //    "webkitBugs": []
  // }
  //
  // Caller is responsible for deleting the returned value.
  Value* GetFeatureStatus();

  std::string GetBlacklistVersion() const;

  void AddLogMessage(Value* msg);

  const ListValue& log_messages() const;

  // Can be called on any thread.
  GpuFeatureFlags GetGpuFeatureFlags();

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

  // Gives ownership of the built-in blacklist.  This is always called on the
  // UI thread.
  void SetGpuBlacklist(GpuBlacklist* gpu_blacklist);

  // This gets called when switching GPU might have happened.
  void HandleGpuSwitch();

  // Returns the Gpu Info as a DictionaryValue.
  DictionaryValue* GpuInfoAsDictionaryValue() const;

  // Returns true if the software rendering should currently be used.
  bool software_rendering();

  // Register a path to the SwiftShader software renderer.
  void RegisterSwiftShaderPath(FilePath path);

 private:
  class UserFlags {
   public:
    UserFlags();

    void Initialize();

    bool disable_accelerated_2d_canvas() const {
      return disable_accelerated_2d_canvas_;
    }

    bool disable_accelerated_compositing() const {
      return disable_accelerated_compositing_;
    }

    bool disable_accelerated_layers() const {
      return disable_accelerated_layers_;
    }

    bool disable_experimental_webgl() const {
      return disable_experimental_webgl_;
    }

    bool disable_gl_multisampling() const { return disable_gl_multisampling_; }

    bool ignore_gpu_blacklist() const { return ignore_gpu_blacklist_; }

    bool skip_gpu_data_loading() const { return skip_gpu_data_loading_; }

    const std::string& use_gl() const { return use_gl_; }

   private:
    // Manage the correlations between switches.
    void ApplyPolicies();

    bool disable_accelerated_2d_canvas_;
    bool disable_accelerated_compositing_;
    bool disable_accelerated_layers_;
    bool disable_experimental_webgl_;
    bool disable_gl_multisampling_;

    bool ignore_gpu_blacklist_;
    bool skip_gpu_data_loading_;

    std::string use_gl_;
  };

  typedef ObserverListThreadSafe<GpuDataManager::Observer>
      GpuDataManagerObserverList;

  friend struct DefaultSingletonTraits<GpuDataManager>;

  GpuDataManager();
  virtual ~GpuDataManager();

  void Initialize();

  // Check if we should go ahead and use gpu blacklist.
  // If not, return NULL; otherwise, update and return the current list.
  GpuBlacklist* GetGpuBlacklist() const;

  // If flags hasn't been set and GPUInfo is available, run through blacklist
  // and compute the flags.
  void UpdateGpuFeatureFlags();

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

  // Determin if accelerated-2d-canvas is supported, which depends on whether
  // lose_context could happen and whether skia is the backend.
  bool supportsAccelerated2dCanvas() const;

  // Try to switch to software rendering, if possible and necessary.
  void EnableSoftwareRenderingIfNecessary();

  bool complete_gpu_info_already_requested_;

  GpuFeatureFlags gpu_feature_flags_;
  GpuFeatureFlags preliminary_gpu_feature_flags_;

  UserFlags user_flags_;

  content::GPUInfo gpu_info_;
  mutable base::Lock gpu_info_lock_;

  scoped_ptr<GpuBlacklist> gpu_blacklist_;

  // Observers.
  const scoped_refptr<GpuDataManagerObserverList> observer_list_;

  ListValue log_messages_;
  bool software_rendering_;

  FilePath swiftshader_path_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManager);
};

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
