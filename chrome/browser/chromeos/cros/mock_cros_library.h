// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_CROS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_CROS_LIBRARY_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/cros_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCrosLibrary : public CrosLibrary {
 public:
  MockCrosLibrary() {}
  virtual ~MockCrosLibrary() {}
  MOCK_METHOD0(EnsureLoaded, bool(void));
  MOCK_METHOD0(EnsureLoaded, const std::string&(void));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_CROS_LIBRARY_H_

