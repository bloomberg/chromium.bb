// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/brightness_library.h"

#include "base/message_loop.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/brightness_bubble.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

class BrightnessLibraryImpl : public BrightnessLibrary {
 public:
  BrightnessLibraryImpl() : brightness_connection_(NULL) {
    if (CrosLibrary::Get()->EnsureLoaded())
      Init();
  }

  ~BrightnessLibraryImpl() {
    if (brightness_connection_) {
      chromeos::DisconnectBrightness(brightness_connection_);
      brightness_connection_ = NULL;
    }
  }

  void AddObserver(Observer* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

 private:
  static void BrightnessChangedHandler(void* object,
                                       int brightness_level) {
    BrightnessLibraryImpl* self = static_cast<BrightnessLibraryImpl*>(object);
    self->OnBrightnessChanged(brightness_level);
  }

  void Init() {
    DCHECK(!brightness_connection_) << "Already intialized";
    brightness_connection_ =
        chromeos::MonitorBrightness(&BrightnessChangedHandler, this);
  }

  void OnBrightnessChanged(int brightness_level) {
    // Make sure we run on the UI thread.
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &BrightnessLibraryImpl::OnBrightnessChanged,
                            brightness_level));
      return;
    }

    FOR_EACH_OBSERVER(Observer,
                      observers_,
                      BrightnessChanged(brightness_level));
  }

  chromeos::BrightnessConnection brightness_connection_;

  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessLibraryImpl);
};

class BrightnessLibraryStubImpl : public BrightnessLibrary {
 public:
  BrightnessLibraryStubImpl() {}
  ~BrightnessLibraryStubImpl() {}
  void AddObserver(Observer* observer) {}
  void RemoveObserver(Observer* observer) {}
};

// static
BrightnessLibrary* BrightnessLibrary::GetImpl(bool stub) {
  if (stub)
    return new BrightnessLibraryStubImpl();
  else
    return new BrightnessLibraryImpl();
}

}  // namespace chromeos

// Needed for NewRunnableMethod() call above.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::BrightnessLibraryImpl);
