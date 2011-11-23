// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/gpu_blacklist_updater.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"

using content::BrowserThread;

namespace {

// Delay on first fetch so we don't interfere with startup.
static const int kStartGpuBlacklistFetchDelay = 6000;

// Delay between calls to update the gpu blacklist (48 hours).
static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;

std::string GetChromeVersionString() {
  chrome::VersionInfo version_info;
  return version_info.is_valid() ? version_info.Version() : "0";
}

}  // namespace anonymous

const char* GpuBlacklistUpdater::kDefaultGpuBlacklistURL =
    "https://ssl.gstatic.com/chrome/config/software_rendering_list.json";

GpuBlacklistUpdater::GpuBlacklistUpdater()
    : WebResourceService(g_browser_process->local_state(),
                         GpuBlacklistUpdater::kDefaultGpuBlacklistURL,
                         false,  // don't append locale to URL
                         chrome::NOTIFICATION_CHROME_END,
                         prefs::kGpuBlacklistUpdate,
                         kStartGpuBlacklistFetchDelay,
                         kCacheUpdateDelay) {
  prefs_->RegisterDictionaryPref(prefs::kGpuBlacklist);
  InitializeGpuBlacklist();
}

GpuBlacklistUpdater::~GpuBlacklistUpdater() { }

// static
void GpuBlacklistUpdater::Setup() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Initialize GpuDataManager instance, which collects preliminary
  // graphics information.
  GpuDataManager::GetInstance();

  // Skip auto updates in tests.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSkipGpuDataLoading))
    return;

  // Initialize GpuBlacklistUpdater, which loads the current blacklist;
  // then Schedule a GPU blacklist auto update.
  GpuBlacklistUpdater* updater =
      g_browser_process->gpu_blacklist_updater();
  DCHECK(updater);
}

void GpuBlacklistUpdater::Unpack(const DictionaryValue& parsed_json) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  prefs_->Set(prefs::kGpuBlacklist, parsed_json);
  UpdateGpuBlacklist(parsed_json, false);
}

void GpuBlacklistUpdater::InitializeGpuBlacklist() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We first load it from the browser resources.
  const base::StringPiece gpu_blacklist_json(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_GPU_BLACKLIST));
  GpuBlacklist* built_in_list = new GpuBlacklist(GetChromeVersionString());
  bool succeed = built_in_list->LoadGpuBlacklist(
      gpu_blacklist_json.as_string(), GpuBlacklist::kCurrentOsOnly);
  DCHECK(succeed);
  GpuDataManager::GetInstance()->SetBuiltInGpuBlacklist(built_in_list);

  // Then we check if the cached version is more up-to-date.
  const DictionaryValue* gpu_blacklist_cache =
      prefs_->GetDictionary(prefs::kGpuBlacklist);
  DCHECK(gpu_blacklist_cache);
  UpdateGpuBlacklist(*gpu_blacklist_cache, true);
}

void GpuBlacklistUpdater::UpdateGpuBlacklist(
    const DictionaryValue& gpu_blacklist_cache, bool preliminary) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<GpuBlacklist> gpu_blacklist(
      new GpuBlacklist(GetChromeVersionString()));
  bool success = gpu_blacklist->LoadGpuBlacklist(
      gpu_blacklist_cache, GpuBlacklist::kCurrentOsOnly);
  if (success) {
    GpuDataManager::GetInstance()->UpdateGpuBlacklist(
        gpu_blacklist.release(), preliminary);
  }
}
