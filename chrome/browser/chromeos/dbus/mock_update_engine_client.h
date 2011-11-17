// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_MOCK_UPDATE_ENGINE_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_MOCK_UPDATE_ENGINE_CLIENT_H_

#include <string>

#include "chrome/browser/chromeos/dbus/update_engine_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockUpdateEngineClient : public UpdateEngineClient {
 public:
  MockUpdateEngineClient();
  virtual ~MockUpdateEngineClient();

  MOCK_METHOD1(AddObserver, void(Observer*));
  MOCK_METHOD1(RemoveObserver, void(Observer*));
  MOCK_METHOD1(HasObserver, bool(Observer*));
  MOCK_METHOD1(RequestUpdateCheck, void(UpdateCheckCallback));
  MOCK_METHOD0(RebootAfterUpdate, void());
  MOCK_METHOD1(SetReleaseTrack, void(const std::string&));
  MOCK_METHOD1(GetReleaseTrack, void(GetReleaseTrackCallback));
  MOCK_METHOD0(GetLastStatus, Status());
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_MOCK_UPDATE_ENGINE_CLIENT_H_
