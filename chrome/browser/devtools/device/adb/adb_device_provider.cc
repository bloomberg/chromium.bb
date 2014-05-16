// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/adb/adb_device_provider.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/adb/adb_client_socket.h"
#include "chrome/browser/devtools/device/adb/adb_device_info_query.h"

namespace {

const char kHostDevicesCommand[] = "host:devices";
const char kHostTransportCommand[] = "host:transport:%s|%s";
const char kLocalAbstractCommand[] = "localabstract:%s";

const int kAdbPort = 5037;

class AdbDeviceImpl : public AndroidDeviceManager::Device {
 public:
  AdbDeviceImpl(const std::string& serial, bool is_connected);
  virtual void QueryDeviceInfo(const DeviceInfoCallback& callback) OVERRIDE;

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  virtual ~AdbDeviceImpl() {}

  void RunCommand(const std::string& command,
                  const CommandCallback& callback);
};

AdbDeviceImpl::AdbDeviceImpl(const std::string& serial, bool is_connected)
    : Device(serial, is_connected) {
}

void AdbDeviceImpl::QueryDeviceInfo(const DeviceInfoCallback& callback) {
  AdbDeviceInfoQuery::Start(
      base::Bind(&AdbDeviceImpl::RunCommand, this), callback);
}

void AdbDeviceImpl::OpenSocket(const std::string& name,
                               const SocketCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  AdbClientSocket::TransportQuery(kAdbPort, serial(), socket_name, callback);
}

void AdbDeviceImpl::RunCommand(const std::string& command,
                               const CommandCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string query = base::StringPrintf(kHostTransportCommand,
                                         serial().c_str(), command.c_str());
  AdbClientSocket::AdbQuery(kAdbPort, query, callback);
}

// static
void ReceivedAdbDevices(
    const AdbDeviceProvider::QueryDevicesCallback& callback,
    int result_code,
    const std::string& response) {
  AndroidDeviceManager::Devices result;
  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  for (size_t i = 0; i < serials.size(); ++i) {
    std::vector<std::string> tokens;
    Tokenize(serials[i], "\t ", &tokens);
    bool offline = tokens.size() > 1 && tokens[1] == "offline";
    result.push_back(new AdbDeviceImpl(tokens[0], !offline));
  }
  callback.Run(result);
}

} // namespace

AdbDeviceProvider::~AdbDeviceProvider() {
}

void AdbDeviceProvider::QueryDevices(const QueryDevicesCallback& callback) {
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand, base::Bind(&ReceivedAdbDevices, callback));
}
