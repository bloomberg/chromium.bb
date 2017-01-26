// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_debug_daemon_client.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/chromeos_switches.h"

namespace {

const char kCrOSTracingAgentName[] = "cros";
const char kCrOSTraceLabel[] = "systemTraceEvents";

}  // namespace

namespace chromeos {

FakeDebugDaemonClient::FakeDebugDaemonClient()
    : featues_mask_(DebugDaemonClient::DEV_FEATURE_NONE),
      service_is_available_(true) {
}

FakeDebugDaemonClient::~FakeDebugDaemonClient() {}

void FakeDebugDaemonClient::Init(dbus::Bus* bus) {}

void FakeDebugDaemonClient::DumpDebugLogs(
    bool is_compressed,
    int file_descriptor,
    const GetDebugLogsCallback& callback) {
  callback.Run(true);
}

void FakeDebugDaemonClient::SetDebugMode(const std::string& subsystem,
                                         const SetDebugModeCallback& callback) {
  callback.Run(false);
}

std::string FakeDebugDaemonClient::GetTracingAgentName() {
  return kCrOSTracingAgentName;
}

std::string FakeDebugDaemonClient::GetTraceEventLabel() {
  return kCrOSTraceLabel;
}

void FakeDebugDaemonClient::StartAgentTracing(
    const base::trace_event::TraceConfig& trace_config,
    const StartAgentTracingCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, GetTracingAgentName(), true /* success */));
}

void FakeDebugDaemonClient::StopAgentTracing(
    const StopAgentTracingCallback& callback) {
  std::string no_data;
  callback.Run(GetTracingAgentName(), GetTraceEventLabel(),
               base::RefCountedString::TakeString(&no_data));
}

void FakeDebugDaemonClient::SetStopAgentTracingTaskRunner(
    scoped_refptr<base::TaskRunner> task_runner) {}

void FakeDebugDaemonClient::GetRoutes(bool numeric,
                                      bool ipv6,
                                      const GetRoutesCallback& callback) {
  std::vector<std::string> empty;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, empty));
}

void FakeDebugDaemonClient::GetNetworkStatus(
    const GetNetworkStatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetModemStatus(
    const GetModemStatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetWiMaxStatus(
    const GetWiMaxStatusCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetNetworkInterfaces(
    const GetNetworkInterfacesCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetPerfOutput(
    base::TimeDelta duration,
    const std::vector<std::string>& perf_args,
    int file_descriptor,
    const DBusMethodErrorCallback& error_callback) {}

void FakeDebugDaemonClient::GetScrubbedLogs(const GetLogsCallback& callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Scrubbed Log"] = "Your email address is xxxxxxxx";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, sample));
}

void FakeDebugDaemonClient::GetScrubbedBigLogs(
    const GetLogsCallback& callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Scrubbed Big Log"] = "Your email address is xxxxxxxx";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, sample));
}

void FakeDebugDaemonClient::GetAllLogs(const GetLogsCallback& callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Log"] = "Your email address is abc@abc.com";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, sample));
}

void FakeDebugDaemonClient::GetUserLogFiles(const GetLogsCallback& callback) {
  std::map<std::string, std::string> user_logs;
  user_logs["preferences"] = "Preferences";
  user_logs["invalid_file"] = "Invalid File";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, true, user_logs));
}

void FakeDebugDaemonClient::TestICMP(const std::string& ip_address,
                                     const TestICMPCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::TestICMPWithOptions(
    const std::string& ip_address,
    const std::map<std::string, std::string>& options,
    const TestICMPCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::UploadCrashes() {
}

void FakeDebugDaemonClient::EnableDebuggingFeatures(
    const std::string& password,
    const DebugDaemonClient::EnableDebuggingCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

void FakeDebugDaemonClient::QueryDebuggingFeatures(
    const DebugDaemonClient::QueryDevFeaturesCallback& callback) {
  bool supported = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kSystemDevMode);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          callback, true,
          static_cast<int>(
              supported ? featues_mask_
                        : debugd::DevFeatureFlag::DEV_FEATURES_DISABLED)));
}

void FakeDebugDaemonClient::RemoveRootfsVerification(
    const DebugDaemonClient::EnableDebuggingCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

void FakeDebugDaemonClient::WaitForServiceToBeAvailable(
    const WaitForServiceToBeAvailableCallback& callback) {
  if (service_is_available_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::Bind(callback, true));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(callback);
  }
}

void FakeDebugDaemonClient::SetOomScoreAdj(
    const std::map<pid_t, int32_t>& pid_to_oom_score_adj,
    const SetOomScoreAdjCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::SetDebuggingFeaturesStatus(int featues_mask) {
  featues_mask_ = featues_mask;
}

void FakeDebugDaemonClient::SetServiceIsAvailable(bool is_available) {
  service_is_available_ = is_available;
  if (!is_available)
    return;

  std::vector<WaitForServiceToBeAvailableCallback> callbacks;
  callbacks.swap(pending_wait_for_service_to_be_available_callbacks_);
  for (size_t i = 0; i < callbacks.size(); ++i)
    callbacks[i].Run(is_available);
}

void FakeDebugDaemonClient::CupsAddPrinter(
    const std::string& name,
    const std::string& uri,
    const std::string& ppd_path,
    bool ipp_everywhere,
    const DebugDaemonClient::LegacyCupsAddPrinterCallback& callback,
    const base::Closure& error_callback) {
  printers_.insert(name);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, true));
}

void FakeDebugDaemonClient::CupsAddManuallyConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const std::string& ppd_contents,
    const DebugDaemonClient::CupsAddPrinterCallback& callback,
    const base::Closure& error_callback) {
  printers_.insert(name);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, 0));
}

void FakeDebugDaemonClient::CupsAddAutoConfiguredPrinter(
    const std::string& name,
    const std::string& uri,
    const DebugDaemonClient::CupsAddPrinterCallback& callback,
    const base::Closure& error_callback) {
  printers_.insert(name);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(callback, 0));
}

void FakeDebugDaemonClient::CupsRemovePrinter(
    const std::string& name,
    const DebugDaemonClient::CupsRemovePrinterCallback& callback,
    const base::Closure& error_callback) {
  const bool has_printer = base::ContainsKey(printers_, name);
  if (has_printer)
    printers_.erase(name);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, has_printer));
}

}  // namespace chromeos
