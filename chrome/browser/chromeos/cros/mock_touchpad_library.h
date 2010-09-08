// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_TOUCHPAD_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_TOUCHPAD_LIBRARY_H_
#pragma once

#include "chrome/browser/chromeos/cros/touchpad_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockTouchpadLibrary : public TouchpadLibrary {
 public:
  MockTouchpadLibrary() {}
  virtual ~MockTouchpadLibrary() {}
  MOCK_METHOD1(SetSensitivity, void(int));
  MOCK_METHOD1(SetTapToClick, void(bool));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_TOUCHPAD_LIBRARY_H_

