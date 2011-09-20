// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/brightness_library.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"
#include "third_party/cros/chromeos_brightness.h"

namespace chromeos {

class BrightnessLibraryImpl : public BrightnessLibrary {
 public:
  BrightnessLibraryImpl() : brightness_connection_(NULL) {}

  virtual ~BrightnessLibraryImpl() {
    if (brightness_connection_) {
      chromeos::DisconnectBrightness(brightness_connection_);
      brightness_connection_ = NULL;
    }
  }

  // Begin BrightnessLibrary implementation.
  virtual void Init() OVERRIDE {
    DCHECK(CrosLibrary::Get()->libcros_loaded());
    CHECK(!brightness_connection_) << "Already intialized";
    brightness_connection_ =
        chromeos::MonitorBrightnessV2(&BrightnessChangedHandler, this);
  }

  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {
    chromeos::DecreaseScreenBrightness(allow_off);
  }

  virtual void IncreaseScreenBrightness() OVERRIDE {
    chromeos::IncreaseScreenBrightness();
  }
  // End BrightnessLibrary implementation.

 private:
  static void BrightnessChangedHandler(void* object,
                                       int brightness_level,
                                       bool user_initiated) {
    BrightnessLibraryImpl* self = static_cast<BrightnessLibraryImpl*>(object);
    self->OnBrightnessChanged(brightness_level, user_initiated);
  }

  void OnBrightnessChanged(int brightness_level, bool user_initiated) {
    // Make sure we run on the UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &BrightnessLibraryImpl::OnBrightnessChanged,
                            brightness_level,
                            user_initiated));
      return;
    }

    FOR_EACH_OBSERVER(Observer, observers_,
                      BrightnessChanged(brightness_level, user_initiated));
  }

  chromeos::BrightnessConnection brightness_connection_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessLibraryImpl);
};

class BrightnessLibraryStubImpl : public BrightnessLibrary {
 public:
  BrightnessLibraryStubImpl() {}
  virtual ~BrightnessLibraryStubImpl() {}
  virtual void Init() OVERRIDE {}
  virtual void AddObserver(Observer* observer) OVERRIDE {}
  virtual void RemoveObserver(Observer* observer) OVERRIDE {}
  virtual void DecreaseScreenBrightness(bool allow_off) OVERRIDE {}
  virtual void IncreaseScreenBrightness() OVERRIDE {}
};

// static
BrightnessLibrary* BrightnessLibrary::GetImpl(bool stub) {
  BrightnessLibrary* impl;
  if (stub)
    impl = new BrightnessLibraryStubImpl();
  else
    impl = new BrightnessLibraryImpl();
  impl->Init();
  return impl;
}

}  // namespace chromeos

// Needed for NewRunnableMethod() call above.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::BrightnessLibraryImpl);
