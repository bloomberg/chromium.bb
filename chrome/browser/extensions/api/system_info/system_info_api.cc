// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info/system_info_api.h"

#include <set>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/system_info_cpu/cpu_info_provider.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/common/extensions/api/experimental_system_info_cpu.h"
#include "chrome/common/extensions/api/experimental_system_info_storage.h"
#include "ui/gfx/display_observer.h"

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

// Event router for systemInfo API. It is a singleton instance shared by
// multiple profiles.
// TODO(hongbo): It should derive from SystemMonitor::DevicesChangedObserver.
// Since the system_monitor will be refactored along with media_gallery, once
// http://crbug.com/145400 is fixed, we need to update SystemInfoEventRouter
// accordingly.
class SystemInfoEventRouter
    : public gfx::DisplayObserver, public StorageInfoObserver {
 public:
  static SystemInfoEventRouter* GetInstance();

  // Add/remove event listener for the |event_name| event from |profile|.
  void AddEventListener(const std::string& event_name);
  void RemoveEventListener(const std::string& event_name);

  // Return true if the |event_name| is an event from systemInfo namespace.
  static bool IsSystemInfoEvent(const std::string& event_name);

  // StorageInfoObserver implementation:
  virtual void OnStorageFreeSpaceChanged(const std::string& id,
                                         double new_value,
                                         double old_value) OVERRIDE;

  // TODO(hongbo): The following methods should be likely overriden from
  // SystemMonitor::DevicesChangedObserver once the http://crbug.com/145400
  // is fixed.
  void OnRemovableStorageAttached(const std::string& id,
                                  const string16& name,
                                  const base::FilePath::StringType& location);
  void OnRemovableStorageDetached(const std::string& id);

  // gfx::DisplayObserver implementation.
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SystemInfoEventRouter>;
  friend class base::RefCountedThreadSafe<SystemInfoEventRouter>;

  SystemInfoEventRouter();
  virtual ~SystemInfoEventRouter();

  // Called from any thread to dispatch the systemInfo event to all extension
  // processes cross multiple profiles.
  void DispatchEvent(const std::string& event_name,
      scoped_ptr<base::ListValue> args);

  // The callbacks of querying storage info to start and stop watching the
  // storages. Called from UI thread.
  void StartWatchingStorages(const StorageInfo& info, bool success);
  void StopWatchingStorages(const StorageInfo& info, bool success);

  // The callback for CPU sampling cycle. Called from FILE thread.
  void OnNextCpuSampling(
      scoped_ptr<api::experimental_system_info_cpu::CpuUpdateInfo> info);

  // Called to dispatch the systemInfo.display.onDisplayChanged event.
  void OnDisplayChanged();

  // Used to record the event names being watched.
  std::multiset<std::string> watching_event_set_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoEventRouter);
};

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

}  // namespace

static base::LazyInstance<ProfileKeyedAPIFactory<SystemInfoAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<SystemInfoAPI>* SystemInfoAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

SystemInfoAPI::SystemInfoAPI(Profile* profile) : profile_(profile) {
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnCpuUpdated);
  ExtensionSystem::Get(profile_)->event_router()->RegisterObserver(
      this, event_names::kOnStorageAvailableCapacityChanged);
}

SystemInfoAPI::~SystemInfoAPI() {
}

void SystemInfoAPI::Shutdown() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

void SystemInfoAPI::OnListenerAdded(const EventListenerInfo& details) {
  SystemInfoEventRouter::GetInstance()->AddEventListener(details.event_name);
}

void SystemInfoAPI::OnListenerRemoved(const EventListenerInfo& details) {
  SystemInfoEventRouter::GetInstance()->RemoveEventListener(details.event_name);
}

}  // namespace extensions
