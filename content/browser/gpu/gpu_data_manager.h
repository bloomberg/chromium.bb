// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
#define CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
#pragma once

#include <set>
#include <string>

#include "base/callback_old.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/values.h"
#include "content/common/gpu/gpu_feature_flags.h"
#include "content/common/gpu/gpu_info.h"

class CommandLine;
class GpuBlacklist;

class GpuDataManager {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManager* GetInstance();

  // Requests complete GPUinfo if it has not already been requested
  void RequestCompleteGpuInfoIfNeeded();

  // Only update if the current GPUInfo is not finalized.
  void UpdateGpuInfo(const GPUInfo& gpu_info);

  const GPUInfo& gpu_info() const;

  // Returns status of various GPU features. Return type is
  // GpuBlacklist::GetFeatureStatus, or NULL if blacklist is
  // uninitialized. Caller is responsible for deleting the returned value.
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

  // Add a callback.
  void AddGpuInfoUpdateCallback(Callback0::Type* callback);

  // Remove a callback.
  // Returns true if removed, or false if it was not found.
  bool RemoveGpuInfoUpdateCallback(Callback0::Type* callback);

  // Inserting disable-feature switches into renderer process command-line
  // in correspondance to preliminary gpu feature flags.
  void AppendRendererCommandLine(CommandLine* command_line);

  // Gives ownership of the latest blacklist.  This is always called on the UI
  // thread.
  void UpdateGpuBlacklist(GpuBlacklist* gpu_blacklist);

 private:
  friend struct DefaultSingletonTraits<GpuDataManager>;

  GpuDataManager();
  virtual ~GpuDataManager();

  // Check if we should go ahead and use gpu blacklist.
  // If not, return NULL; otherwise, update and return the current list.
  GpuBlacklist* GetGpuBlacklist();

  // If flags hasn't been set and GPUInfo is available, run through blacklist
  // and compute the flags.
  void UpdateGpuFeatureFlags();

  // Call all callbacks.
  void RunGpuInfoUpdateCallbacks();

  bool complete_gpu_info_already_requested_;

  bool gpu_feature_flags_set_;
  GpuFeatureFlags gpu_feature_flags_;

  GPUInfo gpu_info_;
  mutable base::Lock gpu_info_lock_;

  scoped_ptr<GpuBlacklist> gpu_blacklist_;

  // Map of callbacks.
  std::set<Callback0::Type*> gpu_info_update_callbacks_;

  ListValue log_messages_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManager);
};

DISABLE_RUNNABLE_METHOD_REFCOUNT(GpuDataManager);

#endif  // CONTENT_BROWSER_GPU_GPU_DATA_MANAGER_H_
