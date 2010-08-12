// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/synaptics_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

class SynapticsLibraryImpl : public SynapticsLibrary {
 public:
  SynapticsLibraryImpl() {}
  virtual ~SynapticsLibraryImpl() {}

  void SetBoolParameter(SynapticsParameter param, bool value) {
    SetParameter(param, value ? 1 : 0);
  }

  void SetRangeParameter(SynapticsParameter param, int value) {
    if (value < 1)
      value = 1;
    if (value > 10)
      value = 10;
    SetParameter(param, value);
  }

 private:
  // This helper methods calls into the libcros library to set the parameter.
  // This call is run on the FILE thread.
  void SetParameter(SynapticsParameter param, int value) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      // This calls SetSynapticsParameter in the cros library which is
      // potentially time consuming. So we run this on the FILE thread.
      ChromeThread::PostTask(
          ChromeThread::FILE, FROM_HERE,
          NewRunnableFunction(&SetSynapticsParameter, param, value));
    }
  }

  DISALLOW_COPY_AND_ASSIGN(SynapticsLibraryImpl);
};

class SynapticsLibraryStubImpl : public SynapticsLibrary {
 public:
  SynapticsLibraryStubImpl() {}
  virtual ~SynapticsLibraryStubImpl() {}
  void SetBoolParameter(SynapticsParameter param, bool value) {}
  void SetRangeParameter(SynapticsParameter param, int value) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SynapticsLibraryStubImpl);
};

// static
SynapticsLibrary* SynapticsLibrary::GetImpl(bool stub) {
  if (stub)
    return new SynapticsLibraryStubImpl();
  else
    return new SynapticsLibraryImpl();
}

}  // namespace chromeos
