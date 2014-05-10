// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_debug_daemon_client.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"

namespace chromeos {

FakeDebugDaemonClient::FakeDebugDaemonClient() {}

FakeDebugDaemonClient::~FakeDebugDaemonClient() {}

void FakeDebugDaemonClient::Init(dbus::Bus* bus) {}

void FakeDebugDaemonClient::GetDebugLogs(base::File file,
                                         const GetDebugLogsCallback& callback) {
  callback.Run(false);
}

void FakeDebugDaemonClient::SetDebugMode(const std::string& subsystem,
                                         const SetDebugModeCallback& callback) {
  callback.Run(false);
}
void FakeDebugDaemonClient::StartSystemTracing() {}

bool FakeDebugDaemonClient::RequestStopSystemTracing(
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

}  // namespace chromeos
