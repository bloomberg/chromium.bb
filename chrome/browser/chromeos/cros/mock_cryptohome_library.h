// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_

#include "chrome/browser/chromeos/cros/cryptohome_library.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockCryptohomeLibrary : public CryptohomeLibrary {
 public:
  MockCryptohomeLibrary() {}
  ~MockCryptohomeLibrary() {}
  MOCK_METHOD2(Mount, bool(const std::string& user_email,
                           const std::string& passhash));
  MOCK_METHOD2(CheckKey, bool(const std::string& user_email,
                              const std::string& passhash));
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_CRYPTOHOME_LIBRARY_H_
