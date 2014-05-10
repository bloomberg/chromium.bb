// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/dbus/debug_daemon_client.h"

namespace chromeos {

// The DebugDaemonClient implementation used on Linux desktop,
// which does nothing.
class FakeDebugDaemonClient : public DebugDaemonClient {
 public:
  FakeDebugDaemonClient();
  virtual ~FakeDebugDaemonClient();

  virtual void Init(dbus::Bus* bus) OVERRIDE;
  virtual void GetDebugLogs(base::File file,
                            const GetDebugLogsCallback& callback) OVERRIDE;
  virtual void SetDebugMode(const std::string& subsystem,
                            const SetDebugModeCallback& callback) OVERRIDE;
  virtual void StartSystemTracing() OVERRIDE;
  virtual bool RequestStopSystemTracing(
      const StopSystemTracingCallback& callback) OVERRIDE;
  virtual void GetRoutes(bool numeric,
                         bool ipv6,
                         const GetRoutesCallback& callback) OVERRIDE;
  virtual void GetNetworkStatus(const GetNetworkStatusCallback& callback)
      OVERRIDE;
  virtual void GetModemStatus(const GetModemStatusCallback& callback) OVERRIDE;
  virtual void GetWiMaxStatus(const GetWiMaxStatusCallback& callback) OVERRIDE;
  virtual void GetNetworkInterfaces(
      const GetNetworkInterfacesCallback& callback) OVERRIDE;
  virtual void GetPerfData(uint32_t duration,
                           const GetPerfDataCallback& callback) OVERRIDE;
  virtual void GetScrubbedLogs(const GetLogsCallback& callback) OVERRIDE;
  virtual void GetAllLogs(const GetLogsCallback& callback) OVERRIDE;
  virtual void GetUserLogFiles(const GetLogsCallback& callback) OVERRIDE;
  virtual void TestICMP(const std::string& ip_address,
                        const TestICMPCallback& callback) OVERRIDE;
  virtual void TestICMPWithOptions(
      const std::string& ip_address,
      const std::map<std::string, std::string>& options,
      const TestICMPCallback& callback) OVERRIDE;
  virtual void UploadCrashes() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeDebugDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_DEBUG_DAEMON_CLIENT_H_
