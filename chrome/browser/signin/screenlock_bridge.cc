// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/screenlock_bridge.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#endif

namespace {

base::LazyInstance<ScreenlockBridge> g_screenlock_bridge_bridge_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
ScreenlockBridge* ScreenlockBridge::Get() {
  return g_screenlock_bridge_bridge_instance.Pointer();
}

// static
std::string ScreenlockBridge::GetAuthenticatedUserEmail(Profile* profile) {
  // |profile| has to be a signed-in profile with SigninManager already
  // created. Otherwise, just crash to collect stack.
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);
  return signin_manager->GetAuthenticatedUsername();
}

ScreenlockBridge::ScreenlockBridge() : lock_handler_(NULL) {
}

ScreenlockBridge::~ScreenlockBridge() {
}

void ScreenlockBridge::SetLockHandler(LockHandler* lock_handler) {
  DCHECK(lock_handler_ == NULL || lock_handler == NULL);
  lock_handler_ = lock_handler;
  if (lock_handler_)
    FOR_EACH_OBSERVER(Observer, observers_, OnScreenDidLock());
  else
    FOR_EACH_OBSERVER(Observer, observers_, OnScreenDidUnlock());
}

bool ScreenlockBridge::IsLocked() const {
  return lock_handler_ != NULL;
}

void ScreenlockBridge::Lock(Profile* profile) {
#if defined(OS_CHROMEOS)
  chromeos::SessionManagerClient* session_manager =
      chromeos::DBusThreadManager::Get()->GetSessionManagerClient();
  session_manager->RequestLockScreen();
#else
  profiles::LockProfile(profile);
#endif
}

void ScreenlockBridge::Unlock(Profile* profile) {
  if (lock_handler_)
    lock_handler_->Unlock(GetAuthenticatedUserEmail(profile));
}

void ScreenlockBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenlockBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
