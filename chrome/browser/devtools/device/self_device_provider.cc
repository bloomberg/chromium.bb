// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/self_device_provider.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/socket/tcp_client_socket.h"

namespace {

const char kDeviceModel[] = "Local Chrome";
const char kBrowserName[] = "Chrome";
const char kLocalhost[] = "127.0.0.1";

class SelfAsDevice : public AndroidDeviceManager::Device {
 public:
  explicit SelfAsDevice(int port);

  virtual void QueryDeviceInfo(const DeviceInfoCallback& callback) OVERRIDE;

  virtual void OpenSocket(const std::string& socket_name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  virtual ~SelfAsDevice() {}

  int port_;
};

SelfAsDevice::SelfAsDevice(int port)
    : Device("local", true),
      port_(port)
{}

void SelfAsDevice::QueryDeviceInfo(const DeviceInfoCallback& callback) {
  AndroidDeviceManager::DeviceInfo device_info;
  device_info.model = kDeviceModel;

  AndroidDeviceManager::BrowserInfo browser_info;
  browser_info.socket_name = base::IntToString(port_);
  browser_info.display_name = kBrowserName;
  browser_info.type = AndroidDeviceManager::BrowserInfo::kTypeChrome;

  device_info.browser_info.push_back(browser_info);

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(callback, device_info));
}

static void RunSocketCallback(
    const AndroidDeviceManager::SocketCallback& callback,
    net::StreamSocket* socket,
    int result) {
  callback.Run(result, socket);
}

void SelfAsDevice::OpenSocket(const std::string& socket_name,
                              const SocketCallback& callback) {
  DCHECK(CalledOnValidThread());
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  net::IPAddressNumber ip_number;
  net::ParseIPLiteralToNumber(kLocalhost, &ip_number);
  int port;
  base::StringToInt(socket_name, &port);
  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip_number, port);
  net::TCPClientSocket* socket = new net::TCPClientSocket(
      address_list, NULL, net::NetLog::Source());
  socket->Connect(base::Bind(&RunSocketCallback, callback, socket));
}

} // namespace

SelfAsDeviceProvider::SelfAsDeviceProvider(int port)
    : port_(port) {
}

void SelfAsDeviceProvider::QueryDevices(const QueryDevicesCallback& callback) {
  AndroidDeviceManager::Devices result;
  result.push_back(new SelfAsDevice(port_));
  callback.Run(result);
}
