// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_DEBUG_DAEMON_CLIENT_H_

#include <stdint.h>

#include <map>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task_runner.h"
#include "base/trace_event/tracing_agent.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/dbus_client.h"
#include "dbus/file_descriptor.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// DebugDaemonClient is used to communicate with the debug daemon.
class CHROMEOS_EXPORT DebugDaemonClient
    : public DBusClient,
      public base::trace_event::TracingAgent {
 public:
  ~DebugDaemonClient() override;

  // Called once GetDebugLogs() is complete. Takes one parameter:
  // - succeeded: was the logs stored successfully.
  typedef base::Callback<void(bool succeeded)> GetDebugLogsCallback;

  // Requests to store debug logs into |file| and calls |callback|
  // when completed. Debug logs will be stored in the .tgz if
  // |is_compressed| is true, otherwise in logs will be stored in .tar format.
  virtual void DumpDebugLogs(bool is_compressed,
                             base::File file,
                             scoped_refptr<base::TaskRunner> task_runner,
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

  using DBusMethodErrorCallback =
      base::Callback<void(const std::string& error_name,
                          const std::string& error_message)>;

  // Runs perf (via quipper) with arguments for |duration| (converted to
  // seconds) and returns data collected over the passed |file_descriptor|.
  // |error_callback| is called if there is an error with the DBus call.
  // Note that quipper failures may occur after successfully running the DBus
  // method. Such errors can be detected by |file_descriptor| being closed with
  // no data written.
  virtual void GetPerfOutput(base::TimeDelta duration,
                             const std::vector<std::string>& perf_args,
                             dbus::ScopedFileDescriptor file_descriptor,
                             const DBusMethodErrorCallback& error_callback) = 0;

  // Callback type for GetScrubbedLogs(), GetAllLogs() or GetUserLogFiles().
  using GetLogsCallback =
      base::Callback<void(bool succeeded,
                          const std::map<std::string, std::string>& logs)>;

  // Gets scrubbed logs from debugd.
  virtual void GetScrubbedLogs(const GetLogsCallback& callback) = 0;

  // Gets the scrubbed logs from debugd that are very large and cannot be
  // returned directly from D-Bus. These logs will include ARC and cheets
  // system information.
  virtual void GetScrubbedBigLogs(const GetLogsCallback& callback) = 0;

  // Gets all logs collected by debugd.
  virtual void GetAllLogs(const GetLogsCallback& callback) = 0;

  // Gets list of user log files that must be read by Chrome.
  virtual void GetUserLogFiles(const GetLogsCallback& callback) = 0;

  virtual void SetStopAgentTracingTaskRunner(
      scoped_refptr<base::TaskRunner> task_runner) = 0;

  // Returns an empty StopAgentTracingCallback that does nothing.
  static StopAgentTracingCallback EmptyStopAgentTracingCallback();

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

  // Called once EnableDebuggingFeatures() is complete. |succeeded| will be true
  // if debugging features have been successfully enabled.
  typedef base::Callback<void(bool succeeded)> EnableDebuggingCallback;

  // Enables debugging features (sshd, boot from USB). |password| is a new
  // password for root user. Can be only called in dev mode.
  virtual void EnableDebuggingFeatures(
      const std::string& password,
      const EnableDebuggingCallback& callback) = 0;

  static const int DEV_FEATURE_NONE = 0;
  static const int DEV_FEATURE_ALL_ENABLED =
      debugd::DevFeatureFlag::DEV_FEATURE_ROOTFS_VERIFICATION_REMOVED |
      debugd::DevFeatureFlag::DEV_FEATURE_BOOT_FROM_USB_ENABLED |
      debugd::DevFeatureFlag::DEV_FEATURE_SSH_SERVER_CONFIGURED |
      debugd::DevFeatureFlag::DEV_FEATURE_DEV_MODE_ROOT_PASSWORD_SET;

  // Called once QueryDebuggingFeatures() is complete. |succeeded| will be true
  // if debugging features have been successfully enabled. |feature_mask| is a
  // bitmask made out of DebuggingFeature enum values.
  typedef base::Callback<void(bool succeeded,
                              int feature_mask)> QueryDevFeaturesCallback;
  // Checks which debugging features have been already enabled.
  virtual void QueryDebuggingFeatures(
      const QueryDevFeaturesCallback& callback) = 0;

  // Removes rootfs verification from the file system. Can be only called in
  // dev mode.
  virtual void RemoveRootfsVerification(
      const EnableDebuggingCallback& callback) = 0;

  // Trigger uploading of crashes.
  virtual void UploadCrashes() = 0;

  // A callback for WaitForServiceToBeAvailable().
  typedef base::Callback<void(bool service_is_ready)>
      WaitForServiceToBeAvailableCallback;

  // Runs the callback as soon as the service becomes available.
  virtual void WaitForServiceToBeAvailable(
      const WaitForServiceToBeAvailableCallback& callback) = 0;

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
