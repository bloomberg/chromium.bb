// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_DEBUG_DAEMON_CLIENT_H_

#include "chromeos/dbus/debug_daemon_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockDebugDaemonClient : public DebugDaemonClient {
 public:
  MockDebugDaemonClient();
  virtual ~MockDebugDaemonClient();

  MOCK_METHOD2(GetDebugLogs, void(base::PlatformFile,
                                  const GetDebugLogsCallback&));
  MOCK_METHOD2(SetDebugMode, void(const std::string&,
                                  const SetDebugModeCallback&));
  MOCK_METHOD3(GetRoutes, void(bool, bool, const GetRoutesCallback&));
  MOCK_METHOD1(GetNetworkStatus, void(const GetNetworkStatusCallback&));
  MOCK_METHOD1(GetModemStatus, void(const GetModemStatusCallback&));
  MOCK_METHOD1(GetNetworkInterfaces, void(const GetNetworkInterfacesCallback&));
  MOCK_METHOD2(GetPerfData, void(uint32_t, const GetPerfDataCallback&));
  MOCK_METHOD1(GetAllLogs, void(const GetLogsCallback&));
  MOCK_METHOD1(GetUserLogFiles, void(const GetLogsCallback&));
  MOCK_METHOD1(RequestStopSystemTracing,
      bool(const StopSystemTracingCallback&));
  MOCK_METHOD0(StartSystemTracing, void());
  MOCK_METHOD2(TestICMP, void(const std::string&, const TestICMPCallback&));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_DEBUG_DAEMON_CLIENT_H_
