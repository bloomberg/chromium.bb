// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_

#include "base/callback.h"
#include "base/files/file.h"
#include "base/memory/ref_counted_memory.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"

#include <map>

namespace metrics {
class PerfDataProto;
}

namespace chromeos {

// DebugDaemonClient is used to communicate with the debug daemon.
class CHROMEOS_EXPORT DebugDaemonClient : public DBusClient {
 public:
  virtual ~DebugDaemonClient();

  // Called once GetDebugLogs() is complete. Takes one parameter:
  // - succeeded: was the logs stored successfully.
  typedef base::Callback<void(bool succeeded)> GetDebugLogsCallback;

  // Requests to store debug logs into |file| and calls |callback|
  // when completed. Debug logs will be stored in the .tgz format.
  virtual void GetDebugLogs(base::File file,
                            const GetDebugLogsCallback& callback) = 0;

  // Called once SetDebugMode() is complete. Takes one parameter:
  // - succeeded: debug mode was changed successfully.
  typedef base::Callback<void(bool succeeded)> SetDebugModeCallback;

  // Requests to change debug mode to given |subsystem| and calls
  // |callback| when completed. |subsystem| should be one of the
  // following: "wifi", "ethernet", "cellular" or "none".
  virtual void SetDebugMode(const std::string& subsystem,
                            const SetDebugModeCallback& callback) = 0;

  // Called once GetRoutes() is complete.
  typedef base::Callback<void(bool succeeded,
                              const std::vector<std::string>& routes)>
      GetRoutesCallback;
  virtual void GetRoutes(bool numeric, bool ipv6,
                         const GetRoutesCallback& callback) = 0;

  // Called once GetNetworkStatus() is complete.
  typedef base::Callback<void(bool succeeded, const std::string& status)>
      GetNetworkStatusCallback;

  // Gets information about network status as json.
  virtual void GetNetworkStatus(const GetNetworkStatusCallback& callback) = 0;

  // Called once GetModemStatus() is complete.
  typedef base::Callback<void(bool succeeded, const std::string& status)>
      GetModemStatusCallback;

  // Gets information about modem status as json.
  virtual void GetModemStatus(const GetModemStatusCallback& callback) = 0;

  // Called once GetWiMaxStatus() is complete.
  typedef base::Callback<void(bool succeeded, const std::string& status)>
      GetWiMaxStatusCallback;

  // Gets information about WiMAX status as json.
  virtual void GetWiMaxStatus(const GetWiMaxStatusCallback& callback) = 0;

  // Called once GetNetworkInterfaces() is complete. Takes two parameters:
  // - succeeded: information was obtained successfully.
  // - status: network interfaces information in json. For details, please refer
  //   to http://gerrit.chromium.org/gerrit/#/c/28045/5/src/helpers/netif.cc
  typedef base::Callback<void(bool succeeded, const std::string& status)>
      GetNetworkInterfacesCallback;

  // Gets information about network interfaces as json.
  virtual void GetNetworkInterfaces(
      const GetNetworkInterfacesCallback& callback) = 0;

  // Called once GetPerfData() is complete only if the the data is successfully
  // obtained from debugd.
  typedef base::Callback<void(const std::vector<uint8>& data)>
      GetPerfDataCallback;

  // Runs perf for |duration| seconds and returns data collected.
  virtual void GetPerfData(uint32_t duration,
                           const GetPerfDataCallback& callback) = 0;

  // Callback type for GetScrubbedLogs(), GetAllLogs() or GetUserLogFiles().
  typedef base::Callback<void(bool succeeded,
                              const std::map<std::string, std::string>& logs)>
      GetLogsCallback;

  // Gets scrubbed logs from debugd.
  virtual void GetScrubbedLogs(const GetLogsCallback& callback) = 0;

  // Gets all logs collected by debugd.
  virtual void GetAllLogs(const GetLogsCallback& callback) = 0;

  // Gets list of user log files that must be read by Chrome.
  virtual void GetUserLogFiles(const GetLogsCallback& callback) = 0;

  // Requests to start system/kernel tracing.
  virtual void StartSystemTracing() = 0;

  // Called once RequestStopSystemTracing() is complete. Takes one parameter:
  // - result: the data collected while tracing was active
  typedef base::Callback<void(const scoped_refptr<base::RefCountedString>&
      result)> StopSystemTracingCallback;

  // Requests to stop system tracing and calls |callback| when completed.
  virtual bool RequestStopSystemTracing(const StopSystemTracingCallback&
      callback) = 0;

  // Returns an empty SystemTracingCallback that does nothing.
  static StopSystemTracingCallback EmptyStopSystemTracingCallback();

  // Called once TestICMP() is complete. Takes two parameters:
  // - succeeded: information was obtained successfully.
  // - status: information about ICMP connectivity to a specified host as json.
  //   For details please refer to
  //   https://gerrit.chromium.org/gerrit/#/c/30310/2/src/helpers/icmp.cc
  typedef base::Callback<void(bool succeeded, const std::string& status)>
      TestICMPCallback;

  // Tests ICMP connectivity to a specified host. The |ip_address| contains the
  // IPv4 or IPv6 address of the host, for example "8.8.8.8".
  virtual void TestICMP(const std::string& ip_address,
                        const TestICMPCallback& callback) = 0;

  // Tests ICMP connectivity to a specified host. The |ip_address| contains the
  // IPv4 or IPv6 address of the host, for example "8.8.8.8".
  virtual void TestICMPWithOptions(
      const std::string& ip_address,
      const std::map<std::string, std::string>& options,
      const TestICMPCallback& callback) = 0;

  // Trigger uploading of crashes.
  virtual void UploadCrashes() = 0;

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via DBusThreadManager::Get().
  static DebugDaemonClient* Create();

 protected:
  // Create() should be used instead.
  DebugDaemonClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(DebugDaemonClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_
