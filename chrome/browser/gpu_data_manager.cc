// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_data_manager.h"

#include "app/app_switches.h"
#include "app/gfx/gl/gl_implementation.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/browser/gpu_blacklist.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

GpuDataManager::GpuDataManager()
        : gpu_feature_flags_set_(false),
          gpu_blacklist_cache_(NULL) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(g_browser_process);
  PrefService* prefs = g_browser_process->local_state();
  // If we bring up chrome normally, prefs should never be NULL; however, we
  // we handle the case where prefs == NULL for certain tests.
  if (prefs) {
    prefs->RegisterDictionaryPref(prefs::kGpuBlacklist);
    gpu_blacklist_cache_ = prefs->GetMutableDictionary(prefs::kGpuBlacklist);
    DCHECK(gpu_blacklist_cache_);

    gpu_blacklist_updater_ = new GpuBlacklistUpdater();
    // TODO(zmo): uncomment the following line to turn on auto-updating.
    // gpu_blacklist_updater_->StartAfterDelay();
  }

  LoadGpuBlacklist();
  UpdateGpuBlacklist();
}

GpuDataManager::~GpuDataManager() { }

GpuDataManager* GpuDataManager::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return Singleton<GpuDataManager>::get();
}

void GpuDataManager::UpdateGpuInfo(const GPUInfo& gpu_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (gpu_info_.level() >= gpu_info.level())
    return;
  gpu_info_ = gpu_info;
  child_process_logging::SetGpuInfo(gpu_info);
  // Clear the flag to triger a re-computation of GpuFeatureFlags using the
  // updated GPU info.
  gpu_feature_flags_set_ = false;
  RunGpuInfoUpdateCallbacks();
}

const GPUInfo& GpuDataManager::gpu_info() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return gpu_info_;
}

GpuFeatureFlags GpuDataManager::GetGpuFeatureFlags() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  UpdateGpuFeatureFlags();
  return gpu_feature_flags_;
}

void GpuDataManager::AddGpuInfoUpdateCallback(Callback0::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gpu_info_update_callbacks_.insert(callback);
}

bool GpuDataManager::RemoveGpuInfoUpdateCallback(Callback0::Type* callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<Callback0::Type*>::iterator i =
      gpu_info_update_callbacks_.find(callback);
  if (i != gpu_info_update_callbacks_.end()) {
    gpu_info_update_callbacks_.erase(i);
    return true;
  }
  return false;
}

void GpuDataManager::RunGpuInfoUpdateCallbacks() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<Callback0::Type*>::iterator i = gpu_info_update_callbacks_.begin();
  for (; i != gpu_info_update_callbacks_.end(); ++i) {
    (*i)->Run();
  }
}

bool GpuDataManager::LoadGpuBlacklist() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (gpu_blacklist_.get() != NULL)
    return true;
  static const base::StringPiece gpu_blacklist_json(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_GPU_BLACKLIST));
  gpu_blacklist_.reset(new GpuBlacklist());
  if (gpu_blacklist_->LoadGpuBlacklist(gpu_blacklist_json.as_string(), true))
    return true;
  gpu_blacklist_.reset(NULL);
  return false;
}

bool GpuDataManager::UpdateGpuBlacklist() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (gpu_blacklist_cache_ == NULL)
    return false;
  uint16 cached_version_major, cached_version_minor;
  if (!GpuBlacklist::GetVersion(*gpu_blacklist_cache_,
                                &cached_version_major,
                                &cached_version_minor))
    return false;
  if (gpu_blacklist_.get() != NULL) {
    uint16 current_version_major, current_version_minor;
    if (gpu_blacklist_->GetVersion(&current_version_major,
                                   &current_version_minor) &&
        (cached_version_major < current_version_major ||
         (cached_version_major == current_version_major &&
          cached_version_minor <= current_version_minor)))
      return false;
  }
  GpuBlacklist* updated_list = new GpuBlacklist();
  if (!updated_list->LoadGpuBlacklist(*gpu_blacklist_cache_, true)) {
    delete updated_list;
    return false;
  }
  gpu_blacklist_.reset(updated_list);
  // Clear the flag to triger a re-computation of GpuFeatureFlags using the
  // updated GPU blacklist.
  gpu_feature_flags_set_ = false;
  return true;
}

void GpuDataManager::UpdateGpuFeatureFlags() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (gpu_info_.level() == GPUInfo::kUninitialized)
    return;

  // Need to call this before checking gpu_feature_flags_set_ because it might
  // be reset if a newer version of GPU blacklist is downloaed.
  GpuBlacklist* gpu_blacklist = GetGpuBlacklist();
  if (gpu_blacklist == NULL)
    return;

  if (gpu_feature_flags_set_)
    return;

  gpu_feature_flags_set_ = true;
  gpu_feature_flags_.set_flags(0);

  if (gpu_blacklist != NULL) {
    gpu_feature_flags_ = gpu_blacklist->DetermineGpuFeatureFlags(
        GpuBlacklist::kOsAny, NULL, gpu_info_);
    uint32 max_entry_id = gpu_blacklist->max_entry_id();
    if (gpu_feature_flags_.flags() != 0) {
      // If gpu is blacklisted, no further GPUInfo will be collected.
      gpu_info_.SetLevel(GPUInfo::kComplete);
      // TODO(zmo): move histograming to GpuBlacklist::DetermineGpuFeatureFlags.
      std::vector<uint32> flag_entries;
      gpu_blacklist->GetGpuFeatureFlagEntries(
          GpuFeatureFlags::kGpuFeatureAll, flag_entries);
      DCHECK_GT(flag_entries.size(), 0u);
      for (size_t i = 0; i < flag_entries.size(); ++i) {
        UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
            flag_entries[i], max_entry_id + 1);
      }
    } else {
      UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
          0, max_entry_id + 1);
    }
  }
}

GpuBlacklist* GpuDataManager::GetGpuBlacklist() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) ||
      browser_command_line.GetSwitchValueASCII(
          switches::kUseGL) == gfx::kGLImplementationOSMesaName)
    return NULL;
  UpdateGpuBlacklist();
  // No need to return an empty blacklist.
  if (gpu_blacklist_.get() != NULL && gpu_blacklist_->max_entry_id() == 0)
    return NULL;
  return gpu_blacklist_.get();
}

