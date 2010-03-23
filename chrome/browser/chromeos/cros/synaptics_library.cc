// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/synaptics_library.h"

#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

void SynapticsLibraryImpl::SetBoolParameter(SynapticsParameter param,
                                            bool value) {
  SetParameter(param, value ? 1 : 0);
}

void SynapticsLibraryImpl::SetRangeParameter(SynapticsParameter param,
                                             int value) {
  if (value < 1)
    value = 1;
  if (value > 10)
    value = 10;
  SetParameter(param, value);
}

void SynapticsLibraryImpl::SetParameter(SynapticsParameter param, int value) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    // This calls SetSynapticsParameter in the cros library which is
    // potentially time consuming. So we run this on the FILE thread.
    ChromeThread::PostTask(
        ChromeThread::FILE, FROM_HERE,
        NewRunnableFunction(&SetSynapticsParameter, param, value));
  }
}

}  // namespace chromeos
