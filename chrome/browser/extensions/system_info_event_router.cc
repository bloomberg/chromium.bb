// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_info_event_router.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/common/extensions/api/experimental_system_info_cpu.h"
#include "chrome/common/extensions/api/experimental_system_info_storage.h"

#if defined(USE_ASH)
#include "ash/screen_ash.h"
#include "ash/shell.h"
#endif

namespace extensions {

using api::experimental_system_info_cpu::CpuUpdateInfo;
using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::StorageChangeInfo;
using content::BrowserThread;

namespace {

// The display events use the "systemInfo" prefix.
const char kSystemInfoEventPrefix[] = "systemInfo";
const char kDisplayEventPrefix[] = "systemInfo.display";

// The storage and cpu events still use the "experimental.systemInfo" prefix.
const char kExperimentalSystemInfoEventPrefix[] = "experimental.systemInfo";
const char kStorageEventPrefix[] = "experimental.systemInfo.storage";
const char kCpuEventPrefix[] = "experimental.systemInfo.cpu";

static bool IsDisplayEvent(const std::string& event_name) {
  return StartsWithASCII(event_name, kDisplayEventPrefix, true);
}

static bool IsStorageEvent(const std::string& event_name) {
  return StartsWithASCII(event_name, kStorageEventPrefix, true);
}

static bool IsCpuEvent(const std::string& event_name) {
  return StartsWithASCII(event_name, kCpuEventPrefix, true);
}

}  // namespace

// static
SystemInfoEventRouter* SystemInfoEventRouter::GetInstance() {
  return Singleton<SystemInfoEventRouter>::get();
}

SystemInfoEventRouter::SystemInfoEventRouter() {
}

SystemInfoEventRouter::~SystemInfoEventRouter() {
}

void SystemInfoEventRouter::StartWatchingStorages(
    const StorageInfo& info, bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success)
    return;

  for (StorageInfo::const_iterator it = info.begin(); it != info.end(); ++it) {
    StorageInfoProvider::Get()->StartWatching((*it)->id);
  }
}

void SystemInfoEventRouter::StopWatchingStorages(
    const StorageInfo& info, bool success) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!success)
    return;

  for (StorageInfo::const_iterator it = info.begin(); it != info.end(); ++it) {
    StorageInfoProvider::Get()->StopWatching((*it)->id);
  }
}

void SystemInfoEventRouter::AddEventListener(const std::string& event_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  watching_event_set_.insert(event_name);
  if (watching_event_set_.count(event_name) > 1)
    return;
  // Start watching the |event_name| event if the first event listener arrives.
  // For systemInfo.storage event.
  if (IsStorageEvent(event_name)) {
    StorageInfoProvider::Get()->AddObserver(this);
    StorageInfoProvider::Get()->StartQueryInfo(
        base::Bind(&SystemInfoEventRouter::StartWatchingStorages,
                   base::Unretained(this)));
    return;
  }

  // For systemInfo.cpu event.
  if (IsCpuEvent(event_name)) {
    CpuInfoProvider::Get()->StartSampling(
        base::Bind(&SystemInfoEventRouter::OnNextCpuSampling,
                   base::Unretained(this)));
    return;
  }

  // For systemInfo.display event.
  if (IsDisplayEvent(event_name)) {
#if defined(USE_ASH)
    ash::Shell::GetScreen()->AddObserver(this);
#endif
  }
}

void SystemInfoEventRouter::RemoveEventListener(
    const std::string& event_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  watching_event_set_.erase(event_name);
  if (watching_event_set_.count(event_name) > 0)
    return;

  // In case of the last event listener is removed, we need to stop watching
  // it to avoid unnecessary overhead.
  if (IsStorageEvent(event_name)) {
    StorageInfoProvider::Get()->StartQueryInfo(
        base::Bind(&SystemInfoEventRouter::StopWatchingStorages,
                   base::Unretained(this)));
    StorageInfoProvider::Get()->RemoveObserver(this);
  }

  if (IsCpuEvent(event_name)) {
    CpuInfoProvider::Get()->StopSampling();
  }

  if (IsDisplayEvent(event_name)) {
#if defined(USE_ASH)
    ash::Shell::GetScreen()->RemoveObserver(this);
#endif
  }
}

// static
bool SystemInfoEventRouter::IsSystemInfoEvent(const std::string& event_name) {
  // TODO(hshi): simplify this once all systemInfo APIs are out of experimental.
  return (StartsWithASCII(event_name, kSystemInfoEventPrefix, true) ||
          StartsWithASCII(event_name, kExperimentalSystemInfoEventPrefix,
                          true));
}

// Called on UI thread since the observer is added from UI thread.
void SystemInfoEventRouter::OnStorageFreeSpaceChanged(
    const std::string& id, double new_value, double old_value) {
  StorageChangeInfo info;
  info.id = id;
  info.available_capacity = static_cast<double>(new_value);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(info.ToValue().release());

  DispatchEvent(event_names::kOnStorageAvailableCapacityChanged, args.Pass());
}

void SystemInfoEventRouter::OnRemovableStorageAttached(const std::string& id,
    const string16& name,  const base::FilePath::StringType& location) {
  // TODO(hongbo): Handle storage device arrival/removal event.
}

void SystemInfoEventRouter::OnRemovableStorageDetached(const std::string& id) {
  // TODO(hongbo): Same as above.
}

void SystemInfoEventRouter::OnDisplayBoundsChanged(
    const gfx::Display& display) {
  OnDisplayChanged();
}

void SystemInfoEventRouter::OnDisplayAdded(const gfx::Display& new_display) {
  OnDisplayChanged();
}

void SystemInfoEventRouter::OnDisplayRemoved(const gfx::Display& old_display) {
  OnDisplayChanged();
}

void SystemInfoEventRouter::OnDisplayChanged() {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  DispatchEvent(event_names::kOnDisplayChanged, args.Pass());
}

void SystemInfoEventRouter::DispatchEvent(const std::string& event_name,
                                          scoped_ptr<base::ListValue> args) {
  g_browser_process->extension_event_router_forwarder()->
      BroadcastEventToRenderers(event_name, args.Pass(), GURL());
}

void SystemInfoEventRouter::OnNextCpuSampling(scoped_ptr<CpuUpdateInfo> info) {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(info->ToValue().release());

  DispatchEvent(event_names::kOnCpuUpdated, args.Pass());
}

}  // namespace extensions
