// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/screen_lock_library.h"

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

// This class handles the interaction with the ChromeOS screen lock APIs.
class ScreenLockLibraryImpl : public ScreenLockLibrary {
 public:
  ScreenLockLibraryImpl() {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      Init();
    }
  }

  ~ScreenLockLibraryImpl() {
    if (screen_lock_connection_) {
      chromeos::DisconnectScreenLock(screen_lock_connection_);
    }
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  void NotifyScreenLockRequested() {
    chromeos::NotifyScreenLockRequested();
  }

  void NotifyScreenLockCompleted() {
    chromeos::NotifyScreenLockCompleted();
  }

  void NotifyScreenUnlockRequested() {
    chromeos::NotifyScreenUnlockRequested();
  }

  void NotifyScreenUnlockCompleted() {
    chromeos::NotifyScreenUnlockCompleted();
  }

 private:
  void Init() {
    screen_lock_connection_ = chromeos::MonitorScreenLock(
        &ScreenLockedHandler, this);
  }

  void LockScreen() {
    // Make sure we run on UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ScreenLockLibraryImpl::LockScreen));
      return;
    }
    FOR_EACH_OBSERVER(Observer, observers_, LockScreen(this));
  }

  void UnlockScreen() {
    // Make sure we run on UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ScreenLockLibraryImpl::UnlockScreen));
      return;
    }
    FOR_EACH_OBSERVER(Observer, observers_, UnlockScreen(this));
  }

  void UnlockScreenFailed() {
    // Make sure we run on UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ScreenLockLibraryImpl::UnlockScreenFailed));
      return;
    }
    FOR_EACH_OBSERVER(Observer, observers_, UnlockScreenFailed(this));
  }

  static void ScreenLockedHandler(void* object, ScreenLockEvent event) {
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

  ObserverList<Observer> observers_;

  // A reference to the screen lock api
  chromeos::ScreenLockConnection screen_lock_connection_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockLibraryImpl);
};

class ScreenLockLibraryStubImpl : public ScreenLockLibrary {
 public:
  ScreenLockLibraryStubImpl() {}
  ~ScreenLockLibraryStubImpl() {}
  void AddObserver(Observer* observer) {}
  void RemoveObserver(Observer* observer) {}
  void NotifyScreenLockRequested() {}
  void NotifyScreenLockCompleted() {}
  void NotifyScreenUnlockRequested() {}
  void NotifyScreenUnlockCompleted() {}
};

// static
ScreenLockLibrary* ScreenLockLibrary::GetImpl(bool stub) {
  if (stub)
    return new ScreenLockLibraryStubImpl();
  else
    return new ScreenLockLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::ScreenLockLibraryImpl);
