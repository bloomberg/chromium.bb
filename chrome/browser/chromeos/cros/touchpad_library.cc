// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/touchpad_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "content/browser/browser_thread.h"

namespace chromeos {

class TouchpadLibraryImpl : public TouchpadLibrary {
 public:
  TouchpadLibraryImpl() {}
  virtual ~TouchpadLibraryImpl() {}

  void SetSensitivity(int value) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableFunction(&SetTouchpadSensitivity, value));
    }
  }

  void SetTapToClick(bool enabled) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableFunction(&SetTouchpadTapToClick, enabled));
    }
  }

  DISALLOW_COPY_AND_ASSIGN(TouchpadLibraryImpl);
};

class TouchpadLibraryStubImpl : public TouchpadLibrary {
 public:
  TouchpadLibraryStubImpl() {}
  virtual ~TouchpadLibraryStubImpl() {}
  void SetSensitivity(int value) {}
  void SetTapToClick(bool enabled) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchpadLibraryStubImpl);
};

// static
TouchpadLibrary* TouchpadLibrary::GetImpl(bool stub) {
  if (stub)
    return new TouchpadLibraryStubImpl();
  else
    return new TouchpadLibraryImpl();
}

}  // namespace chromeos
