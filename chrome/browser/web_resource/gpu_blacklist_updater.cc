// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/gpu_blacklist_updater.h"

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "content/browser/browser_thread.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
static const int kStartGpuBlacklistFetchDelay = 6000;

// Delay between calls to update the gpu blacklist (48 hours).
static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;

}  // namespace

const char* GpuBlacklistUpdater::kDefaultGpuBlacklistURL =
    "https://dl.google.com/dl/edgedl/chrome/gpu/software_rendering_list.json";

GpuBlacklistUpdater::GpuBlacklistUpdater()
    : WebResourceService(ProfileManager::GetDefaultProfile(),
                         g_browser_process->local_state(),
                         GpuBlacklistUpdater::kDefaultGpuBlacklistURL,
                         false,  // don't append locale to URL
                         NotificationType::NOTIFICATION_TYPE_COUNT,
                         prefs::kGpuBlacklistUpdate,
                         kStartGpuBlacklistFetchDelay,
                         kCacheUpdateDelay) {
}

GpuBlacklistUpdater::~GpuBlacklistUpdater() { }

void GpuBlacklistUpdater::Unpack(const DictionaryValue& parsed_json) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DictionaryValue* gpu_blacklist_cache =
      prefs_->GetMutableDictionary(prefs::kGpuBlacklist);
  DCHECK(gpu_blacklist_cache);
  gpu_blacklist_cache->Clear();
  gpu_blacklist_cache->MergeDictionary(&parsed_json);
}

