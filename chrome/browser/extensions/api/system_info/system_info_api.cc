// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/system_info/system_info_api.h"

#include <set>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/system_storage/storage_info_provider.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/common/extensions/api/system_display.h"
#include "chrome/common/extensions/api/system_storage.h"
#include "components/storage_monitor/removable_storage_observer.h"
#include "components/storage_monitor/storage_info.h"
#include "components/storage_monitor/storage_monitor.h"
#include "content/public/browser/browser_thread.h"
#include "ui/gfx/display_observer.h"

#if defined(OS_CHROMEOS)
#include "ash/shell.h"
#include "ui/gfx/screen.h"
#endif

namespace extensions {

using api::system_storage::StorageUnitInfo;
using content::BrowserThread;
using storage_monitor::StorageMonitor;

namespace system_display = api::system_display;
namespace system_storage = api::system_storage;

namespace {

#if defined(OS_CHROMEOS)
bool IsDisplayChangedEvent(const std::string& event_name) {
  return event_name == system_display::OnDisplayChanged::kEventName;
}
#endif

bool IsSystemStorageEvent(const std::string& event_name) {
  return (event_name == system_storage::OnAttached::kEventName ||
          event_name == system_storage::OnDetached::kEventName);
}

// Event router for systemInfo API. It is a singleton instance shared by
// multiple profiles.
class SystemInfoEventRouter : public gfx::DisplayObserver,
                              public storage_monitor::RemovableStorageObserver {
 public:
  static SystemInfoEventRouter* GetInstance();

  SystemInfoEventRouter();
  virtual ~SystemInfoEventRouter();

  // Add/remove event listener for the |event_name| event.
  void AddEventListener(const std::string& event_name);
  void RemoveEventListener(const std::string& event_name);

 private:
  // gfx::DisplayObserver:
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

  // RemovableStorageObserver implementation.
  virtual void OnRemovableStorageAttached(
      const storage_monitor::StorageInfo& info) OVERRIDE;
  virtual void OnRemovableStorageDetached(
      const storage_monitor::StorageInfo& info) OVERRIDE;

  // Called from any thread to dispatch the systemInfo event to all extension
  // processes cross multiple profiles.
  void DispatchEvent(const std::string& event_name,
                     scoped_ptr<base::ListValue> args);

  // Called to dispatch the systemInfo.display.onDisplayChanged event.
  void OnDisplayChanged();

  // Used to record the event names being watched.
  std::multiset<std::string> watching_event_set_;

  bool has_storage_monitor_observer_;

  DISALLOW_COPY_AND_ASSIGN(SystemInfoEventRouter);
};

static base::LazyInstance<SystemInfoEventRouter>::Leaky
    g_system_info_event_router = LAZY_INSTANCE_INITIALIZER;

// static
SystemInfoEventRouter* SystemInfoEventRouter::GetInstance() {
  return g_system_info_event_router.Pointer();
}

SystemInfoEventRouter::SystemInfoEventRouter()
    : has_storage_monitor_observer_(false) {
}

SystemInfoEventRouter::~SystemInfoEventRouter() {
  if (has_storage_monitor_observer_) {
    StorageMonitor* storage_monitor = StorageMonitor::GetInstance();
    if (storage_monitor)
      storage_monitor->RemoveObserver(this);
  }
}

void SystemInfoEventRouter::AddEventListener(const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  watching_event_set_.insert(event_name);
  if (watching_event_set_.count(event_name) > 1)
    return;

  // For system.display event.
#if defined(OS_CHROMEOS)
  if (IsDisplayChangedEvent(event_name))
    ash::Shell::GetScreen()->AddObserver(this);
#endif

  if (IsSystemStorageEvent(event_name)) {
    if (!has_storage_monitor_observer_) {
      has_storage_monitor_observer_ = true;
      DCHECK(StorageMonitor::GetInstance()->IsInitialized());
      StorageMonitor::GetInstance()->AddObserver(this);
    }
  }
}

void SystemInfoEventRouter::RemoveEventListener(const std::string& event_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  std::multiset<std::string>::iterator it =
      watching_event_set_.find(event_name);
  if (it != watching_event_set_.end()) {
    watching_event_set_.erase(it);
    if (watching_event_set_.count(event_name) > 0)
      return;
  }

#if defined(OS_CHROMEOS)
  if (IsDisplayChangedEvent(event_name))
    ash::Shell::GetScreen()->RemoveObserver(this);
#endif

  if (IsSystemStorageEvent(event_name)) {
    const std::string& other_event_name =
        (event_name == system_storage::OnDetached::kEventName) ?
            system_storage::OnAttached::kEventName :
            system_storage::OnDetached::kEventName;
    if (watching_event_set_.count(other_event_name) == 0) {
      StorageMonitor::GetInstance()->RemoveObserver(this);
      has_storage_monitor_observer_ = false;
    }
  }
}

void SystemInfoEventRouter::OnRemovableStorageAttached(
    const storage_monitor::StorageInfo& info) {
  StorageUnitInfo unit;
  systeminfo::BuildStorageUnitInfo(info, &unit);
  scoped_ptr<base::ListValue> args(new base::ListValue);
  args->Append(unit.ToValue().release());
  DispatchEvent(system_storage::OnAttached::kEventName, args.Pass());
}

void SystemInfoEventRouter::OnRemovableStorageDetached(
    const storage_monitor::StorageInfo& info) {
  scoped_ptr<base::ListValue> args(new base::ListValue);
  std::string transient_id =
      StorageMonitor::GetInstance()->GetTransientIdForDeviceId(
          info.device_id());
  args->AppendString(transient_id);

  DispatchEvent(system_storage::OnDetached::kEventName, args.Pass());
}

void SystemInfoEventRouter::OnDisplayAdded(const gfx::Display& new_display) {
  OnDisplayChanged();
}

void SystemInfoEventRouter::OnDisplayRemoved(const gfx::Display& old_display) {
  OnDisplayChanged();
}

void SystemInfoEventRouter::OnDisplayMetricsChanged(const gfx::Display& display,
                                                    uint32_t metrics) {
  OnDisplayChanged();
}

void SystemInfoEventRouter::OnDisplayChanged() {
  scoped_ptr<base::ListValue> args(new base::ListValue());
  DispatchEvent(system_display::OnDisplayChanged::kEventName, args.Pass());
}

void SystemInfoEventRouter::DispatchEvent(const std::string& event_name,
                                          scoped_ptr<base::ListValue> args) {
  g_browser_process->extension_event_router_forwarder()->
      BroadcastEventToRenderers(event_name, args.Pass(), GURL());
}

void AddEventListener(const std::string& event_name) {
  SystemInfoEventRouter::GetInstance()->AddEventListener(event_name);
}

void RemoveEventListener(const std::string& event_name) {
  SystemInfoEventRouter::GetInstance()->RemoveEventListener(event_name);
}

}  // namespace

static base::LazyInstance<BrowserContextKeyedAPIFactory<SystemInfoAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<SystemInfoAPI>*
SystemInfoAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

SystemInfoAPI::SystemInfoAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* router = EventRouter::Get(browser_context_);
  router->RegisterObserver(this, system_storage::OnAttached::kEventName);
  router->RegisterObserver(this, system_storage::OnDetached::kEventName);
  router->RegisterObserver(this, system_display::OnDisplayChanged::kEventName);
}

SystemInfoAPI::~SystemInfoAPI() {
}

void SystemInfoAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void SystemInfoAPI::OnListenerAdded(const EventListenerInfo& details) {
  if (IsSystemStorageEvent(details.event_name)) {
    StorageMonitor::GetInstance()->EnsureInitialized(
        base::Bind(&AddEventListener, details.event_name));
  } else {
    AddEventListener(details.event_name);
  }
}

void SystemInfoAPI::OnListenerRemoved(const EventListenerInfo& details) {
  if (IsSystemStorageEvent(details.event_name)) {
    StorageMonitor::GetInstance()->EnsureInitialized(
        base::Bind(&RemoveEventListener, details.event_name));
  } else {
    RemoveEventListener(details.event_name);
  }
}

}  // namespace extensions
