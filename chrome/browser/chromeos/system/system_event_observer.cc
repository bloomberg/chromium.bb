// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/system_event_observer.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/extensions/system/system_api.h"

namespace chromeos {
namespace system {

namespace {

SystemEventObserver* g_system_event_observer = NULL;

}

SystemEventObserver::SystemEventObserver() {
}

SystemEventObserver::~SystemEventObserver() {
}

void SystemEventObserver::SystemResumed() {
  extensions::DispatchWokeUpEvent();
}

void SystemEventObserver::UnlockScreen() {
  extensions::DispatchScreenUnlockedEvent();
}

// static
void SystemEventObserver::Initialize() {
  DCHECK(!g_system_event_observer);
  g_system_event_observer = new SystemEventObserver();
  VLOG(1) << "SystemEventObserver initialized";
  DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(g_system_event_observer);
}

// static
SystemEventObserver* SystemEventObserver::GetInstance() {
  return g_system_event_observer;
}

// static
void SystemEventObserver::Shutdown() {
  DCHECK(g_system_event_observer);
  DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(g_system_event_observer);
  delete g_system_event_observer;
  g_system_event_observer = NULL;
  VLOG(1) << "SystemEventObserver Shutdown completed";
}

}  // namespace system
}  // namespace chromeos
