// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_data_manager.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/common/content_client.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/gpu/gpu_info_collector.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"

GpuDataManager::GpuDataManager()
    : complete_gpu_info_already_requested_(false) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GPUInfo gpu_info;
  gpu_info_collector::CollectPreliminaryGraphicsInfo(&gpu_info);
  UpdateGpuInfo(gpu_info);
}

GpuDataManager::~GpuDataManager() { }

GpuDataManager* GpuDataManager::GetInstance() {
  return Singleton<GpuDataManager>::get();
}

void GpuDataManager::RequestCompleteGpuInfoIfNeeded() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (complete_gpu_info_already_requested_)
    return;
  complete_gpu_info_already_requested_ = true;

  GpuProcessHost::SendOnIO(
      0,
      content::CAUSE_FOR_GPU_LAUNCH_GPUDATAMANAGER_REQUESTCOMPLETEGPUINFOIFNEEDED,
      new GpuMsg_CollectGraphicsInfo());
}

void GpuDataManager::UpdateGpuInfo(const GPUInfo& gpu_info) {
  base::AutoLock auto_lock(gpu_info_lock_);
  if (!gpu_info_.Merge(gpu_info))
    return;

  content::GetContentClient()->SetGpuInfo(gpu_info_);
}

const GPUInfo& GpuDataManager::gpu_info() const {
  base::AutoLock auto_lock(gpu_info_lock_);
  return gpu_info_;
}

Value* GpuDataManager::GetFeatureStatus() {
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (gpu_blacklist_.get())
    return gpu_blacklist_->GetFeatureStatus(GpuAccessAllowed(),
        browser_command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
        browser_command_line.HasSwitch(switches::kEnableAccelerated2dCanvas),
        browser_command_line.HasSwitch(switches::kDisableExperimentalWebGL),
        browser_command_line.HasSwitch(switches::kDisableGLMultisampling));
  return NULL;
}

std::string GpuDataManager::GetBlacklistVersion() const {
  if (gpu_blacklist_.get() != NULL) {
    uint16 version_major, version_minor;
    if (gpu_blacklist_->GetVersion(&version_major,
                                   &version_minor)) {
      std::string version_string =
          base::UintToString(static_cast<unsigned>(version_major)) +
          "." +
          base::UintToString(static_cast<unsigned>(version_minor));
      return version_string;
    }
  }
  return "";
}

void GpuDataManager::AddLogMessage(Value* msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  log_messages_.Append(msg);
}

const ListValue& GpuDataManager::log_messages() const {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return log_messages_;
}

GpuFeatureFlags GpuDataManager::GetGpuFeatureFlags() {
  return gpu_feature_flags_;
}

bool GpuDataManager::GpuAccessAllowed() {
  uint32 flags = gpu_feature_flags_.flags();

  // This will in effect block access to all GPU features if any of them
  // is blacklisted.
  // TODO(vangelis): Restructure the code to make it possible to selectively
  // blaclist gpu features.
  return !(flags & GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas ||
           flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing ||
           flags & GpuFeatureFlags::kGpuFeatureWebgl);
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

void GpuDataManager::AppendRendererCommandLine(
    CommandLine* command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(command_line);

  uint32 flags = gpu_feature_flags_.flags();
  if ((flags & GpuFeatureFlags::kGpuFeatureWebgl) &&
      !command_line->HasSwitch(switches::kDisableExperimentalWebGL))
    command_line->AppendSwitch(switches::kDisableExperimentalWebGL);
  if ((flags & GpuFeatureFlags::kGpuFeatureMultisampling) &&
      !command_line->HasSwitch(switches::kDisableGLMultisampling))
    command_line->AppendSwitch(switches::kDisableGLMultisampling);
  // If we have kGpuFeatureAcceleratedCompositing, we disable all GPU features.
  if (flags & GpuFeatureFlags::kGpuFeatureAcceleratedCompositing) {
    const char* switches[] = {
        switches::kDisableAcceleratedCompositing,
        switches::kDisableExperimentalWebGL
    };
    const int switch_count = sizeof(switches) / sizeof(char*);
    for (int i = 0; i < switch_count; ++i) {
      if (!command_line->HasSwitch(switches[i]))
        command_line->AppendSwitch(switches[i]);
    }
  }
}

void GpuDataManager::UpdateGpuBlacklist(GpuBlacklist* gpu_blacklist) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gpu_blacklist_.reset(gpu_blacklist);
  UpdateGpuFeatureFlags();
}

void GpuDataManager::RunGpuInfoUpdateCallbacks() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::set<Callback0::Type*>::iterator i = gpu_info_update_callbacks_.begin();
  for (; i != gpu_info_update_callbacks_.end(); ++i) {
    (*i)->Run();
  }
}

void GpuDataManager::UpdateGpuFeatureFlags() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GpuBlacklist* gpu_blacklist = GetGpuBlacklist();
  if (gpu_blacklist == NULL)
    return;

  // We don't set a lock around modifying gpu_feature_flags_ since it's just an
  // int.
  if (!gpu_blacklist) {
    gpu_feature_flags_.set_flags(0);
    return;
  }

  {
    base::AutoLock auto_lock(gpu_info_lock_);
    gpu_feature_flags_ = gpu_blacklist->DetermineGpuFeatureFlags(
        GpuBlacklist::kOsAny, NULL, gpu_info_);

    // If gpu is blacklisted, no further GPUInfo will be collected.
    gpu_info_.finalized = true;
  }

  uint32 max_entry_id = gpu_blacklist->max_entry_id();
  if (!gpu_feature_flags_.flags()) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        0, max_entry_id + 1);
    return;
  }

  // Notify clients that GpuInfo state has changed
  RunGpuInfoUpdateCallbacks();

  // TODO(zmo): move histograming to GpuBlacklist::DetermineGpuFeatureFlags.
  std::vector<uint32> flag_entries;
  gpu_blacklist->GetGpuFeatureFlagEntries(
      GpuFeatureFlags::kGpuFeatureAll, flag_entries);
  DCHECK_GT(flag_entries.size(), 0u);
  for (size_t i = 0; i < flag_entries.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        flag_entries[i], max_entry_id + 1);
  }
}

GpuBlacklist* GpuDataManager::GetGpuBlacklist() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kIgnoreGpuBlacklist) ||
      browser_command_line.GetSwitchValueASCII(
          switches::kUseGL) == gfx::kGLImplementationOSMesaName)
    return NULL;
  // No need to return an empty blacklist.
  if (gpu_blacklist_.get() != NULL && gpu_blacklist_->max_entry_id() == 0)
    return NULL;
  return gpu_blacklist_.get();
}
