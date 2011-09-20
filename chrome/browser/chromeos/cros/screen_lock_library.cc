// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/screen_lock_library.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"
#include "third_party/cros/chromeos_screen_lock.h"

namespace chromeos {

class ScreenLockLibraryImpl : public ScreenLockLibrary {
 public:
  ScreenLockLibraryImpl() : screen_lock_connection_(NULL) {}

  virtual ~ScreenLockLibraryImpl() {
    if (screen_lock_connection_) {
      chromeos::DisconnectScreenLock(screen_lock_connection_);
      screen_lock_connection_ = NULL;
    }
  }

  // Begin ScreenLockLibrary implementation.
  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    screen_lock_connection_ =
        chromeos::MonitorScreenLock(&ScreenLockedHandler, this);
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual void NotifyScreenLockRequested() OVERRIDE {
    chromeos::NotifyScreenLockRequested();
  }

  virtual void NotifyScreenLockCompleted() OVERRIDE {
    chromeos::NotifyScreenLockCompleted();
  }

  virtual void NotifyScreenUnlockRequested() OVERRIDE {
    chromeos::NotifyScreenUnlockRequested();
  }

  virtual void NotifyScreenUnlockCompleted() OVERRIDE {
    chromeos::NotifyScreenUnlockCompleted();
  }
  // End ScreenLockLibrary implementation.

 private:
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
        break;
    }
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

  chromeos::ScreenLockConnection screen_lock_connection_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenLockLibraryImpl);
};

class ScreenLockLibraryStubImpl : public ScreenLockLibrary {
 public:
  ScreenLockLibraryStubImpl() {}
  virtual ~ScreenLockLibraryStubImpl() {}
  virtual void Init() OVERRIDE {}
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual void NotifyScreenLockRequested() OVERRIDE {}
  virtual void NotifyScreenLockCompleted() OVERRIDE {}
  virtual void NotifyScreenUnlockRequested() OVERRIDE {}
  virtual void NotifyScreenUnlockCompleted() OVERRIDE {}
};

// static
ScreenLockLibrary* ScreenLockLibrary::GetImpl(bool stub) {
  ScreenLockLibrary* impl;
  if (stub)
    impl = new ScreenLockLibraryStubImpl();
  else
    impl = new ScreenLockLibraryImpl();
  impl->Init();
  return impl;
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::ScreenLockLibraryImpl);
