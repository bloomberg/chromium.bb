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
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::ScreenLockLibraryImpl);

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

void ScreenLockLibraryImpl::NotifyScreenLockRequested() {
  // Make sure we run on IO thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &ScreenLockLibraryImpl::NotifyScreenLockRequested));
    return;
  }
  chromeos::NotifyScreenLockRequested();
}

void ScreenLockLibraryImpl::NotifyScreenLockCompleted() {
  // Make sure we run on IO thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &ScreenLockLibraryImpl::NotifyScreenLockCompleted));
    return;
  }
  chromeos::NotifyScreenLockCompleted();
}

void ScreenLockLibraryImpl::NotifyScreenUnlockRequested() {
  // Make sure we run on IO thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &ScreenLockLibraryImpl::NotifyScreenUnlockRequested));
    return;
  }
  chromeos::NotifyScreenUnlockRequested();
}

void ScreenLockLibraryImpl::NotifyScreenUnlockCompleted() {
  // Make sure we run on IO thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::IO)) {
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(this,
                          &ScreenLockLibraryImpl::NotifyScreenUnlockCompleted));
    return;
  }
  chromeos::NotifyScreenUnlockCompleted();
}

void ScreenLockLibraryImpl::Init() {
  screen_lock_connection_ = chromeos::MonitorScreenLock(
      &ScreenLockedHandler, this);
}

void ScreenLockLibraryImpl::LockScreen() {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ScreenLockLibraryImpl::LockScreen));
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_, LockScreen(this));
}

void ScreenLockLibraryImpl::UnlockScreen() {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ScreenLockLibraryImpl::UnlockScreen));
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_, UnlockScreen(this));
}

void ScreenLockLibraryImpl::UnlockScreenFailed() {
  // Make sure we run on UI thread.
  if (!ChromeThread::CurrentlyOn(ChromeThread::UI)) {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ScreenLockLibraryImpl::UnlockScreenFailed));
    return;
  }
  FOR_EACH_OBSERVER(Observer, observers_, UnlockScreenFailed(this));
}

// static
void ScreenLockLibraryImpl::ScreenLockedHandler(void* object,
                                                ScreenLockEvent event) {
  ScreenLockLibraryImpl* self = static_cast<ScreenLockLibraryImpl*>(object);
  switch (event) {
    case chromeos::LockScreen:
      self->LockScreen();
      break;
    case chromeos::UnlockScreen:
      self->UnlockScreen();
      break;
    case chromeos::UnlockScreenFailed:
      self->UnlockScreenFailed();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace chromeos
