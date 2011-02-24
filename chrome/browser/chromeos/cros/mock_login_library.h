// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_MOCK_LOGIN_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_MOCK_LOGIN_LIBRARY_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/cros/login_library.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockLoginLibrary : public LoginLibrary {
 public:
  MockLoginLibrary() {}
  virtual ~MockLoginLibrary() {}
  MOCK_METHOD2(CheckWhitelist, bool(const std::string&, std::vector<uint8>*));
  MOCK_METHOD0(EmitLoginPromptReady, bool(void));
  MOCK_METHOD1(EnumerateWhitelisted, bool(std::vector<std::string>*));
  MOCK_METHOD3(RetrieveProperty, bool(const std::string&,
                                      std::string*,
                                      std::vector<uint8>*));
  MOCK_METHOD4(StorePropertyAsync, bool(const std::string&,
                                        const std::string&,
                                        const std::vector<uint8>&,
                                        Delegate*));
  MOCK_METHOD3(UnwhitelistAsync, bool(const std::string&,
                                      const std::vector<uint8>&,
                                      Delegate*));
  MOCK_METHOD3(WhitelistAsync, bool(const std::string&,
                                    const std::vector<uint8>&,
                                    Delegate*));
  MOCK_METHOD2(StartSession, bool(const std::string&, const std::string&));
  MOCK_METHOD1(StopSession, bool(const std::string&));
  MOCK_METHOD0(RestartEntd, bool(void));
  MOCK_METHOD2(RestartJob, bool(int, const std::string&));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_MOCK_LOGIN_LIBRARY_H_
