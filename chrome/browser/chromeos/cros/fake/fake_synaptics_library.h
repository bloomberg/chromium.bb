// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_FAKE_SYNAPTICS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_FAKE_SYNAPTICS_LIBRARY_H_

#include "chrome/browser/chromeos/cros/synaptics_library.h"

namespace chromeos {

class FakeSynapticsLibrary : public SynapticsLibrary {
 public:
  FakeSynapticsLibrary() {}
  virtual ~FakeSynapticsLibrary() {}
  virtual void SetBoolParameter(SynapticsParameter param, bool value) {}
  virtual void SetRangeParameter(SynapticsParameter param, int value) {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_FAKE_SYNAPTICS_LIBRARY_H_
