// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_info_event_router.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/common/extensions/api/experimental_system_info_cpu.h"
#include "chrome/common/extensions/api/experimental_system_info_storage.h"

namespace extensions {

using api::experimental_system_info_cpu::CpuUpdateInfo;
using api::experimental_system_info_storage::StorageUnitInfo;
using api::experimental_system_info_storage::StorageChangeInfo;
using content::BrowserThread;

namespace {

const char kSystemInfoEventPrefix[] = "experimental.systemInfo";
const char kStorageEventPrefix[] = "experimental.systemInfo.storage";
const char kCpuEventPrefix[] = "experimental.systemInfo.cpu";

static bool IsStorageEvent(const std::string& event_name) {
  return event_name.compare(0, strlen(kStorageEventPrefix),
      kStorageEventPrefix) == 0;
}

static bool IsCpuEvent(const std::string& event_name) {
  return event_name.compare(0, strlen(kCpuEventPrefix), kCpuEventPrefix) == 0;
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

void SystemInfoEventRouter::AddEventListener(const std::string& event_name) {
  watching_event_set_.insert(event_name);
  if (watching_event_set_.count(event_name) > 1)
    return;
  // Start watching the |event_name| event if the first event listener arrives.
  // For systemInfo.storage event.
  if (IsStorageEvent(event_name)) {
    // TODO (hongbo): Start watching storage device information via calling
    // SystemMonitor::StartWatchingStorage.
    return;
  }

  // For systemInfo.cpu event.
  if (IsCpuEvent(event_name)) {
    CpuInfoProvider::Get()->StartSampling(
        base::Bind(&SystemInfoEventRouter::OnNextCpuSampling,
                   base::Unretained(this)));
    return;
  }
}

void SystemInfoEventRouter::RemoveEventListener(
    const std::string& event_name) {
  watching_event_set_.erase(event_name);
  if (watching_event_set_.count(event_name) > 0)
    return;

  // In case of the last event listener is removed, we need to stop watching
  // it to avoid unnecessary overhead.
  if (IsStorageEvent(event_name)) {
    // TODO(hongbo): Stop watching storage device information via calling
    // SystemMonitor::StopWatchingStorage.
    return;
  }
  if (IsCpuEvent(event_name)) {
    CpuInfoProvider::Get()->StopSampling();
  }
}

// static
bool SystemInfoEventRouter::IsSystemInfoEvent(const std::string& event_name) {
  std::string prefix(kSystemInfoEventPrefix);
  return event_name.compare(0, prefix.size(), prefix) == 0;
}

void SystemInfoEventRouter::OnStorageAvailableCapacityChanged(
    const std::string& id, int64 available_capacity) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
        base::Bind(&SystemInfoEventRouter::OnStorageAvailableCapacityChanged,
                   base::Unretained(this), id, available_capacity));
    return;
  }

  // We are on the UI thread now.
  StorageChangeInfo info;
  info.id = id;
  info.available_capacity = static_cast<double>(available_capacity);

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Append(info.ToValue().release());

  DispatchEvent(event_names::kOnStorageAvailableCapacityChanged, args.Pass());
}

void SystemInfoEventRouter::OnRemovableStorageAttached(const std::string& id,
    const string16& name,  const FilePath::StringType& location) {
  // TODO(hongbo): Handle storage device arrival/removal event.
}

void SystemInfoEventRouter::OnRemovableStorageDetached(const std::string& id) {
  // TODO(hongbo): Same as above.
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
