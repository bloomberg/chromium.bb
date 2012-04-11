// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_UPDATE_ENGINE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_UPDATE_ENGINE_CLIENT_H_

#include <string>

#include "chromeos/dbus/update_engine_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUpdateEngineClient : public UpdateEngineClient {
 public:
  MockUpdateEngineClient();
  virtual ~MockUpdateEngineClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(HasObserver, bool(Observer*));
  MOCK_METHOD1(RequestUpdateCheck, void(const UpdateCheckCallback&));
  MOCK_METHOD0(RebootAfterUpdate, void());
  MOCK_METHOD1(SetReleaseTrack, void(const std::string&));
  MOCK_METHOD1(GetReleaseTrack, void(const GetReleaseTrackCallback&));
  MOCK_METHOD0(GetLastStatus, Status());
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_UPDATE_ENGINE_CLIENT_H_
