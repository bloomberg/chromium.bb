// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/accessibility/system_event_observer.h"

#include "base/logging.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"

namespace chromeos {
namespace accessibility {

namespace {

SystemEventObserver* g_system_event_observer = NULL;

}

SystemEventObserver::SystemEventObserver() {
  CrosLibrary::Get()->GetPowerLibrary()->AddObserver(this);
  CrosLibrary::Get()->GetScreenLockLibrary()->AddObserver(this);
}

SystemEventObserver::~SystemEventObserver() {
  CrosLibrary::Get()->GetScreenLockLibrary()->RemoveObserver(this);
  CrosLibrary::Get()->GetPowerLibrary()->RemoveObserver(this);
}

void SystemEventObserver::SystemResumed() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  WokeUpEventInfo info(profile);
  SendAccessibilityNotification(
      chrome::NOTIFICATION_ACCESSIBILITY_WOKE_UP, &info);
}

void SystemEventObserver::LockScreen(ScreenLockLibrary* screen_lock_library) {
}

void SystemEventObserver::UnlockScreen(ScreenLockLibrary* screen_lock_library) {
  Profile* profile = ProfileManager::GetDefaultProfile();
  ScreenUnlockedEventInfo info(profile);
  SendAccessibilityNotification(
      chrome::NOTIFICATION_ACCESSIBILITY_SCREEN_UNLOCKED, &info);
}

void SystemEventObserver::UnlockScreenFailed(
    ScreenLockLibrary* screen_lock_library) {
}

// static
void SystemEventObserver::Initialize() {
  DCHECK(!g_system_event_observer);
  g_system_event_observer = new SystemEventObserver();
  VLOG(1) << "SystemEventObserver initialized";
}

// static
SystemEventObserver* SystemEventObserver::GetInstance() {
  return g_system_event_observer;
}

// static
void SystemEventObserver::Shutdown() {
  DCHECK(g_system_event_observer);
  delete g_system_event_observer;
  g_system_event_observer = NULL;
  VLOG(1) << "SystemEventObserver Shutdown completed";
}

}  // namespace accessibility
}  // namespace chromeos
