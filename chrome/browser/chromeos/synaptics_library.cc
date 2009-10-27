// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/synaptics_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros_library.h"

namespace chromeos {

// static
SynapticsLibrary* SynapticsLibrary::Get() {
  return Singleton<SynapticsLibrary>::get();
}

// static
bool SynapticsLibrary::loaded() {
  return CrosLibrary::loaded();
}

void SynapticsLibrary::SetBoolParameter(SynapticsParameter param, bool value) {
  SetParameter(param, value ? 1 : 0);
}

void SynapticsLibrary::SetRangeParameter(SynapticsParameter param, int value) {
  if (value < 1)
    value = 1;
  if (value > 10)
    value = 10;
  SetParameter(param, value);
}

void SynapticsLibrary::SetParameter(SynapticsParameter param, int value) {
  if (CrosLibrary::loaded()) {
    // This calls SetSynapticsParameter in the cros library which is
    // potentially time consuming. So we run this on the FILE thread.
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableFunction(&SetSynapticsParameter, param, value));
  }
}

}  // namespace chromeos
