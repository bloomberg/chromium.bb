// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_SYNAPTICS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_SYNAPTICS_LIBRARY_H_

#include "chrome/browser/chromeos/cros/synaptics_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSynapticsLibrary : public SynapticsLibrary {
 public:
  MockSynapticsLibrary() {}
  virtual ~MockSynapticsLibrary() {}
  MOCK_METHOD2(SetBoolParameter, void(SynapticsParameter, bool));
  MOCK_METHOD2(SetRangeParameter, void(SynapticsParameter, int));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_SYNAPTICS_LIBRARY_H_

