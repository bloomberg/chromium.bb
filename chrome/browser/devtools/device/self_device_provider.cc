// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/self_device_provider.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/socket/tcp_client_socket.h"

namespace {

const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kOpenedUnixSocketsResponse[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\n"
    "00000000: 00000002 00000000 00010000 0001 01 20894 @%s\n";
const char kRemoteDebuggingSocket[] = "chrome_devtools_remote";

const char kDeviceModel[] = "Local Chrome";
const char kLocalhost[] = "127.0.0.1";

class SelfAsDevice : public AndroidDeviceManager::Device {
 public:
  explicit SelfAsDevice(int port);

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE;
  virtual void OpenSocket(const std::string& socket_name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  void RunCommandCallback(const CommandCallback& callback,
                          const std::string& response,
                          int result);

  void RunSocketCallback(const SocketCallback& callback,
                         net::StreamSocket* socket,
                         int result);
  virtual ~SelfAsDevice() {}

  int port_;
};

SelfAsDevice::SelfAsDevice(int port)
    : Device("local", true),
      port_(port)
{}

void SelfAsDevice::RunCommandCallback(const CommandCallback& callback,
                                      const std::string& response,
                                      int result) {
  callback.Run(result, response);
}

void SelfAsDevice::RunSocketCallback(const SocketCallback& callback,
                                     net::StreamSocket* socket,
                                     int result) {
  callback.Run(result, socket);
}

void SelfAsDevice::RunCommand(const std::string& command,
                              const CommandCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string response;
  if (command == kDeviceModelCommand) {
    response = kDeviceModel;
  } else if (command == kOpenedUnixSocketsCommand) {
    response = base::StringPrintf(kOpenedUnixSocketsResponse,
                                  kRemoteDebuggingSocket);
  }

  base::MessageLoop::current()->PostTask(FROM_HERE,
                base::Bind(&SelfAsDevice::RunCommandCallback, this, callback,
                           response, 0));
}

void SelfAsDevice::OpenSocket(const std::string& socket_name,
                              const SocketCallback& callback) {
  DCHECK(CalledOnValidThread());
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  net::IPAddressNumber ip_number;
  net::ParseIPLiteralToNumber(kLocalhost, &ip_number);

  int port = 0;
  if (socket_name == kRemoteDebuggingSocket)
    port = port_;
  else
    base::StringToInt(socket_name, &port);

  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip_number, port);
  net::TCPClientSocket* socket = new net::TCPClientSocket(
      address_list, NULL, net::NetLog::Source());
  socket->Connect(base::Bind(&SelfAsDevice::RunSocketCallback, this, callback,
                             socket));
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
