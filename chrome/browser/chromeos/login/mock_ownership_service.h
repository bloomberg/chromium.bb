// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNERSHIP_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNERSHIP_SERVICE_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/ownership_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockOwnershipService : public OwnershipService {
 public:
  MockOwnershipService();
  virtual ~MockOwnershipService();

  MOCK_METHOD0(IsAlreadyOwned, bool(void));
  MOCK_METHOD1(GetStatus, OwnershipService::Status(bool));
  MOCK_METHOD0(StartLoadOwnerKeyAttempt, void(void));
  MOCK_METHOD2(StartSigningAttempt, void(const std::string&,
                                         OwnerManager::Delegate*));
  MOCK_METHOD3(StartVerifyAttempt, void(const std::string&,
                                        const std::vector<uint8>&,
                                        OwnerManager::Delegate*));
  MOCK_METHOD0(CurrentUserIsOwner, bool(void));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_OWNERSHIP_SERVICE_H_
