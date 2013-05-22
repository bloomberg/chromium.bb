// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/device_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/adb_client_socket.h"

namespace {

const int kAdbPort = 5037;

std::string GetActivityForPackage(const std::string& package) {
  if (package == "org.chromium.chrome.testshell")
    return ".ChromiumTestShellActivity";
  return "com.google.android.apps.chrome.Main";
}

std::string GetDevtoolsSocket(const std::string& package) {
  if (package == "org.chromium.chrome.testshell")
    return "chromium_testshell_devtools_remote";
  return "chrome_devtools_remote";
}

void ReceiveAdbResponse(std::string* response_out, bool* success,
                        base::WaitableEvent* event, int result,
                        const std::string& response) {
  *response_out = response;
  *success = (result >= 0) ? true : false;
  event->Signal();
}

}  // namespace

DeviceManager::DeviceManager() : thread_("Adb_IOThread") {}

DeviceManager::~DeviceManager() {}

Status DeviceManager::StartChrome(
    const std::string& device_serial, const std::string& package, int port) {
  Status status = SetChromeFlags(device_serial);
  if (!status.IsOk())
    return status;
  status = ClearAppData(device_serial, package);
  if (!status.IsOk())
    return status;
  status = Launch(device_serial, package);
  if (!status.IsOk())
    return status;
  return ForwardPort(device_serial, port, GetDevtoolsSocket(package));
}

Status DeviceManager::GetDevices(std::vector<std::string>* devices) {
  std::string response;
  Status status = ExecuteCommand("host:devices", &response);
  if (!status.IsOk())
    return status;
  base::StringTokenizer lines(response, "\n");
  while (lines.GetNext()) {
    std::vector<std::string> fields;
    base::SplitStringAlongWhitespace(lines.token(), &fields);
    if (fields.size() == 2 && fields[1] == "device") {
      devices->push_back(fields[0]);
    }
  }
  return Status(kOk);
}

Status DeviceManager::ForwardPort(
    const std::string& device_serial, int local_port,
    const std::string& remote_abstract) {
  std::string response;
  Status status = ExecuteHostCommand(
      device_serial,
      "forward:tcp:" + base::IntToString(local_port) + ";localabstract:" +
          remote_abstract,
      &response);
  if (!status.IsOk())
    return status;
  if (response == "OKAY")
    return Status(kOk);
  return Status(kUnknownError, "Failed to forward ports: " + response);
}

Status DeviceManager::SetChromeFlags(const std::string& device_serial) {
  std::string response;
  Status status = ExecuteHostShellCommand(
      device_serial,
      "echo chrome --disable-fre --metrics-recording-only "
          "--enable-remote-debugging > /data/local/chrome-command-line;"
          "echo $?",
      &response);
  if (!status.IsOk())
    return status;
  if (response.find("0") == std::string::npos)
    return Status(kUnknownError, "Failed to set Chrome flags");
  return Status(kOk);
}

Status DeviceManager::ClearAppData(const std::string& device_serial,
                                       const std::string& package) {
  std::string response;
  std::string command = "pm clear " + package;
  Status status = ExecuteHostShellCommand(device_serial, command, &response);
  if (!status.IsOk())
    return status;
  if (response.find("Success") == std::string::npos)
    return Status(kUnknownError, "Failed to clear app data: " + response);
  return Status(kOk);
}

Status DeviceManager::Launch(
    const std::string& device_serial, const std::string& package) {
  std::string response;
  Status status = ExecuteHostShellCommand(
      device_serial,
      "am start -a android.intent.action.VIEW -S -W -n " +
          package + "/" + GetActivityForPackage(package) +
          " -d \"data:text/html;charset=utf-8,\"",
      &response);
  if (!status.IsOk())
    return status;
  if (response.find("Complete") == std::string::npos)
    return Status(kUnknownError,
                  "Failed to start " + package + ": " + response);
  return Status(kOk);
}

Status DeviceManager::Init() {
  base::Thread::Options options(base::MessageLoop::TYPE_IO, 0);
  bool thread_started = thread_.StartWithOptions(options);
  if (!thread_started)
    return Status(kUnknownError, "Adb IO thread failed to start");
  return Status(kOk);
}

Status DeviceManager::ExecuteCommand(
    const std::string& command, std::string* response) {
  if (!thread_.IsRunning()) {
    Status status(kOk);
    status = Init();
    if (!status.IsOk())
      return status;
  }
  bool success;
  base::WaitableEvent event(false, false);
  thread_.message_loop_proxy()->PostTask(
      FROM_HERE,
      base::Bind(&DeviceManager::ExecuteCommandOnIOThread,
                 base::Unretained(this), command, response, &success, &event));
  event.Wait();
  LOG(INFO) << "Adb: " << command << " -> " << *response;
  if (success)
    return Status(kOk);
  return Status(kUnknownError, "Adb command failed: " + command);
}

Status DeviceManager::ExecuteHostCommand(
    const std::string& device_serial, const std::string& host_command,
    std::string* response) {
  return ExecuteCommand(
      "host-serial:" + device_serial + ":" + host_command, response);
}

Status DeviceManager::ExecuteHostShellCommand(
    const std::string& device_serial, const std::string& shell_command,
    std::string* response) {
  return ExecuteCommand(
      "host:transport:" + device_serial + "|shell:" + shell_command,
      response);
}

void DeviceManager::ExecuteCommandOnIOThread(
    const std::string& command, std::string* response, bool* success,
    base::WaitableEvent* event) {
  CHECK(MessageLoop::current()->IsType(MessageLoop::TYPE_IO));
  AdbClientSocket::AdbQuery(kAdbPort, command,
      base::Bind(&ReceiveAdbResponse, response, success, event));
}
