// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_resource/gpu_blacklist_updater.h"

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
#include "content/browser/browser_thread.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/browser/gpu/gpu_data_manager.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gl/gl_implementation.h"
#include "ui/gfx/gl/gl_switches.h"

namespace {

// Delay on first fetch so we don't interfere with startup.
static const int kStartGpuBlacklistFetchDelay = 6000;

// Delay between calls to update the gpu blacklist (48 hours).
static const int kCacheUpdateDelay = 48 * 60 * 60 * 1000;

std::string GetChromeVersionString() {
  chrome::VersionInfo version_info;
  std::string rt =  version_info.is_valid() ? version_info.Version() : "0";
  switch (version_info.GetChannel()) {
    case chrome::VersionInfo::CHANNEL_STABLE:
      rt += " stable";
      break;
    case chrome::VersionInfo::CHANNEL_BETA:
      rt += " beta";
      break;
    case chrome::VersionInfo::CHANNEL_DEV:
      rt += " dev";
      break;
    case chrome::VersionInfo::CHANNEL_CANARY:
      rt += " canary";
      break;
    default:
      rt += " unknown";
      break;
  }
  return rt;
}

}  // namespace anonymous

const char* GpuBlacklistUpdater::kDefaultGpuBlacklistURL =
    "https://dl.google.com/dl/edgedl/chrome/gpu/software_rendering_list.json";

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
void GpuBlacklistUpdater::SetupOnFileThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  // Initialize GpuDataManager instance, which collects preliminary
  // graphics information.  This has to happen on FILE thread.
  GpuDataManager::GetInstance();

  // Skip auto updates in tests.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if ((command_line.GetSwitchValueASCII(switches::kUseGL) ==
      gfx::kGLImplementationOSMesaName))
    return;

  // Post GpuBlacklistUpdate task on UI thread.  This has to happen
  // after GpuDataManager is initialized, otherwise it might be
  // initialzed on UI thread.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&GpuBlacklistUpdater::SetupOnUIThread));
}

// static
void GpuBlacklistUpdater::SetupOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Initialize GpuBlacklistUpdater, which loads the current blacklist;
  // then Schedule a GPU blacklist auto update.
  GpuBlacklistUpdater* updater =
      g_browser_process->gpu_blacklist_updater();
  DCHECK(updater);
  updater->StartAfterDelay();
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
