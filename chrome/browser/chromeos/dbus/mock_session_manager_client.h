// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_SESSION_MANAGER_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_SESSION_MANAGER_CLIENT_H_

#include <string>

#include "chrome/browser/chromeos/dbus/session_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSessionManagerClient : public SessionManagerClient {
 public:
  MockSessionManagerClient();
  virtual ~MockSessionManagerClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD0(EmitLoginPromptReady, void(void));
  MOCK_METHOD0(EmitLoginPromptVisible, void(void));
  MOCK_METHOD0(RestartEntd, void(void));
  MOCK_METHOD2(RestartJob, void(int, const std::string&));
  MOCK_METHOD1(StartSession, void(const std::string&));
  MOCK_METHOD0(StopSession, void(void));
  MOCK_METHOD1(RetrieveDevicePolicy, void(RetrievePolicyCallback));
  MOCK_METHOD1(RetrieveUserPolicy, void(RetrievePolicyCallback));
  MOCK_METHOD2(StoreDevicePolicy, void(const std::string&,
                                       StorePolicyCallback));
  MOCK_METHOD2(StoreUserPolicy, void(const std::string&,
                                     StorePolicyCallback));
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_SESSION_MANAGER_CLIENT_H_
