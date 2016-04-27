// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_SESSION_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_SESSION_MANAGER_CLIENT_H_

#include <string>

#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/session_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockSessionManagerClient : public SessionManagerClient {
 public:
  MockSessionManagerClient();
  virtual ~MockSessionManagerClient();

  MOCK_METHOD1(Init, void(dbus::Bus* bus));
  MOCK_METHOD1(SetStubDelegate, void(StubDelegate* delegate));
  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_CONST_METHOD1(HasObserver, bool(const Observer*));
  MOCK_CONST_METHOD0(IsScreenLocked, bool(void));
  MOCK_METHOD0(EmitLoginPromptVisible, void(void));
  MOCK_METHOD1(RestartJob, void(const std::vector<std::string>&));
  MOCK_METHOD1(StartSession, void(const cryptohome::Identification&));
  MOCK_METHOD0(StopSession, void(void));
  MOCK_METHOD0(NotifySupervisedUserCreationStarted, void(void));
  MOCK_METHOD0(NotifySupervisedUserCreationFinished, void(void));
  MOCK_METHOD0(StartDeviceWipe, void(void));
  MOCK_METHOD0(RequestLockScreen, void(void));
  MOCK_METHOD0(NotifyLockScreenShown, void(void));
  MOCK_METHOD0(NotifyLockScreenDismissed, void(void));
  MOCK_METHOD1(RetrieveActiveSessions, void(const ActiveSessionsCallback&));
  MOCK_METHOD1(RetrieveDevicePolicy, void(const RetrievePolicyCallback&));
  MOCK_METHOD2(RetrievePolicyForUser,
               void(const cryptohome::Identification&,
                    const RetrievePolicyCallback&));
  MOCK_METHOD1(BlockingRetrievePolicyForUser,
               std::string(const cryptohome::Identification&));
  MOCK_METHOD2(RetrieveDeviceLocalAccountPolicy,
               void(const std::string&,
                    const RetrievePolicyCallback&));
  MOCK_METHOD2(StoreDevicePolicy,
               void(const std::string&,
                    const StorePolicyCallback&));
  MOCK_METHOD3(StorePolicyForUser,
               void(const cryptohome::Identification&,
                    const std::string&,
                    const StorePolicyCallback&));
  MOCK_METHOD3(StoreDeviceLocalAccountPolicy,
               void(const std::string&,
                    const std::string&,
                    const StorePolicyCallback&));
  MOCK_METHOD2(SetFlagsForUser,
               void(const cryptohome::Identification&,
                    const std::vector<std::string>&));
  MOCK_METHOD1(GetServerBackedStateKeys, void(const StateKeysCallback&));
  MOCK_METHOD1(CheckArcAvailability, void(const ArcCallback&));
  MOCK_METHOD2(StartArcInstance, void(const std::string&, const ArcCallback&));
  MOCK_METHOD1(StopArcInstance, void(const ArcCallback&));
  MOCK_METHOD1(GetArcStartTime, void(const GetArcStartTimeCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_SESSION_MANAGER_CLIENT_H_
