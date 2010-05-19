// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/screen_lock_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
template <>
struct RunnableMethodTraits<chromeos::ScreenLockLibraryImpl> {
  void RetainCallee(chromeos::ScreenLockLibraryImpl* obj) {}
  void ReleaseCallee(chromeos::ScreenLockLibraryImpl* obj) {}
};

namespace chromeos {

ScreenLockLibraryImpl::ScreenLockLibraryImpl() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    Init();
  }
}

ScreenLockLibraryImpl::~ScreenLockLibraryImpl() {
  if (screen_lock_connection_) {
    chromeos::DisconnectScreenLock(screen_lock_connection_);
  }
}

void ScreenLockLibraryImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ScreenLockLibraryImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ScreenLockLibraryImpl::NotifyScreenLockCompleted() {
  chromeos::NotifyScreenLockCompleted();
}

void ScreenLockLibraryImpl::NotifyScreenLockRequested() {
  chromeos::NotifyScreenLockRequested();
}

void ScreenLockLibraryImpl::NotifyScreenUnlocked() {
  chromeos::NotifyScreenUnlocked();
}

void ScreenLockLibraryImpl::Init() {
  screen_lock_connection_ = chromeos::MonitorScreenLock(
      &ScreenLockedHandler, this);
}

void ScreenLockLibraryImpl::ScreenLocked() {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ScreenLockLibraryImpl::ScreenLocked));
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_, ScreenLocked(this));
}

// static
void ScreenLockLibraryImpl::ScreenLockedHandler(void* object, ScreenLockEvent) {
  ScreenLockLibraryImpl* self = static_cast<ScreenLockLibraryImpl*>(object);
  self->ScreenLocked();
}

}  // namespace chromeos
