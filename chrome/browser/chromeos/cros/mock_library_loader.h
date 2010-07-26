// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_LIBRARY_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_LIBRARY_LOADER_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/cros_library_loader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

// Mock for libcros library loader.
class  MockLibraryLoader : public LibraryLoader {
 public:
  MOCK_METHOD1(Load, bool(std::string*));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_LIBRARY_LOADER_H_

