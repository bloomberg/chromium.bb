// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GPU_DATA_MANAGER_H_
#define CHROME_BROWSER_GPU_DATA_MANAGER_H_
#pragma once

#include <set>

#include "base/callback.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/values.h"
#include "chrome/browser/web_resource/gpu_blacklist_updater.h"
#include "chrome/common/gpu_feature_flags.h"
#include "content/common/gpu_info.h"

class CommandLine;
class DictionaryValue;
class GpuBlacklist;

class GpuDataManager {
 public:
  // Getter for the singleton. This will return NULL on failure.
  static GpuDataManager* GetInstance();

  // Requests complete GPUinfo if it has not already been requested
  void RequestCompleteGpuInfoIfNeeded();

  // Only update if the level is higher than the cached GPUInfo level.
  void UpdateGpuInfo(const GPUInfo& gpu_info);

  const GPUInfo& gpu_info() const;

  // Returns blacklisting reasons structure from gpu_blacklist or NULL
  // if not blacklisted. Caller is responsible for deleting returned value.
  Value* GetBlacklistingReasons() const;

  void AddLogMessage(Value* msg);

  const ListValue& log_messages() const;

  // If necessary, compute the flags before returning them.
  GpuFeatureFlags GetGpuFeatureFlags();

  // Add a callback.
  void AddGpuInfoUpdateCallback(Callback0::Type* callback);

  // Remove a callback.
  // Returns true if removed, or false if it was not found.
  bool RemoveGpuInfoUpdateCallback(Callback0::Type* callback);

  // Inserting disable-feature switches into renderer process command-line
  // in correspondance to preliminary gpu feature flags.
  void AppendRendererCommandLine(CommandLine* command_line);

  // If necessary, compute the flags before checking if a feature is allowed
  bool GpuFeatureAllowed(uint32 feature);

 private:
  friend struct DefaultSingletonTraits<GpuDataManager>;

  GpuDataManager();
  virtual ~GpuDataManager();

  bool LoadGpuBlacklist();

  // Check if a newer version of GPU blacklist has been downloaded from the
  // web (and saved in the local state); if yes, use the newer version instead.
  // Return true if a newer version is installed.
  bool UpdateGpuBlacklist();

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
  GpuFeatureFlags preliminary_gpu_feature_flags_;

  GPUInfo gpu_info_;

  scoped_ptr<GpuBlacklist> gpu_blacklist_;

  scoped_refptr<GpuBlacklistUpdater> gpu_blacklist_updater_;
  // This is the version cached in local state that's automatically updated
  // from the web.
  DictionaryValue* gpu_blacklist_cache_;

  // Map of callbacks.
  std::set<Callback0::Type*> gpu_info_update_callbacks_;

  ListValue log_messages_;

  DISALLOW_COPY_AND_ASSIGN(GpuDataManager);
};

#endif  // CHROME_BROWSER_GPU_DATA_MANAGER_H_

