// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_debug_daemon_client.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/chromeos_switches.h"

namespace chromeos {

FakeDebugDaemonClient::FakeDebugDaemonClient()
    : featues_mask_(DebugDaemonClient::DEV_FEATURE_NONE),
      service_is_available_(true) {
}

FakeDebugDaemonClient::~FakeDebugDaemonClient() {}

void FakeDebugDaemonClient::Init(dbus::Bus* bus) {}

void FakeDebugDaemonClient::DumpDebugLogs(
    bool is_compressed,
    base::File file,
    scoped_refptr<base::TaskRunner> task_runner,
    const GetDebugLogsCallback& callback) {
  callback.Run(true);
}

void FakeDebugDaemonClient::SetDebugMode(const std::string& subsystem,
                                         const SetDebugModeCallback& callback) {
  callback.Run(false);
}
void FakeDebugDaemonClient::StartSystemTracing() {}

bool FakeDebugDaemonClient::RequestStopSystemTracing(
    scoped_refptr<base::TaskRunner> task_runner,
    const StopSystemTracingCallback& callback) {
  std::string no_data;
  callback.Run(base::RefCountedString::TakeString(&no_data));
  return true;
}

void FakeDebugDaemonClient::GetRoutes(bool numeric,
                                      bool ipv6,
                                      const GetRoutesCallback& callback) {
  std::vector<std::string> empty;
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, empty));
}

void FakeDebugDaemonClient::GetNetworkStatus(
    const GetNetworkStatusCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetModemStatus(
    const GetModemStatusCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetWiMaxStatus(
    const GetWiMaxStatusCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetNetworkInterfaces(
    const GetNetworkInterfacesCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::GetPerfData(uint32_t duration,
                                        const GetPerfDataCallback& callback) {
  std::vector<uint8> data;
  base::MessageLoop::current()->PostTask(FROM_HERE, base::Bind(callback, data));
}

void FakeDebugDaemonClient::GetScrubbedLogs(const GetLogsCallback& callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Scrubbed Log"] = "Your email address is xxxxxxxx";
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, sample));
}

void FakeDebugDaemonClient::GetAllLogs(const GetLogsCallback& callback) {
  std::map<std::string, std::string> sample;
  sample["Sample Log"] = "Your email address is abc@abc.com";
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, sample));
}

void FakeDebugDaemonClient::GetUserLogFiles(const GetLogsCallback& callback) {
  std::map<std::string, std::string> user_logs;
  user_logs["preferences"] = "Preferences";
  user_logs["invalid_file"] = "Invalid File";
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, true, user_logs));
}

void FakeDebugDaemonClient::TestICMP(const std::string& ip_address,
                                     const TestICMPCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::TestICMPWithOptions(
    const std::string& ip_address,
    const std::map<std::string, std::string>& options,
    const TestICMPCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, false, ""));
}

void FakeDebugDaemonClient::UploadCrashes() {
}

void FakeDebugDaemonClient::EnableDebuggingFeatures(
    const std::string& password,
    const DebugDaemonClient::EnableDebuggingCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, true));
}

void FakeDebugDaemonClient::QueryDebuggingFeatures(
    const DebugDaemonClient::QueryDevFeaturesCallback& callback) {
  bool supported = base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kSystemDevMode);
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(
          callback, true,
          static_cast<int>(
              supported ? featues_mask_
                        : debugd::DevFeatureFlag::DEV_FEATURES_DISABLED)));
}

void FakeDebugDaemonClient::RemoveRootfsVerification(
    const DebugDaemonClient::EnableDebuggingCallback& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::Bind(callback, true));
}

void FakeDebugDaemonClient::WaitForServiceToBeAvailable(
    const WaitForServiceToBeAvailableCallback& callback) {
  if (service_is_available_) {
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, true));
  } else {
    pending_wait_for_service_to_be_available_callbacks_.push_back(callback);
  }
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

}  // namespace chromeos
