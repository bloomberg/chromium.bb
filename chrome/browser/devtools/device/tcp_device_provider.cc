// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/tcp_device_provider.h"

#include <algorithm>

#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/devtools/device/adb/adb_client_socket.h"
#include "net/base/net_errors.h"
#include "net/dns/host_resolver.h"
#include "net/socket/tcp_client_socket.h"

namespace {

const char kDeviceModel[] = "Remote Target";
const char kBrowserName[] = "Target";

static void RunSocketCallback(
    const AndroidDeviceManager::SocketCallback& callback,
    scoped_ptr<net::StreamSocket> socket,
    int result) {
  callback.Run(result, socket.Pass());
}

class ResolveHostAndOpenSocket final {
 public:
  ResolveHostAndOpenSocket(const net::HostPortPair& address,
                           const AdbClientSocket::SocketCallback& callback)
      : callback_(callback) {
    host_resolver_ = net::HostResolver::CreateDefaultResolver(nullptr);
    net::HostResolver::RequestInfo request_info(address);
    int result = host_resolver_->Resolve(
        request_info, net::DEFAULT_PRIORITY, &address_list_,
        base::Bind(&ResolveHostAndOpenSocket::OnResolved,
                   base::Unretained(this)),
        nullptr, net::BoundNetLog());
    if (result != net::ERR_IO_PENDING)
      OnResolved(result);
  }

 private:
  void OnResolved(int result) {
    if (result < 0) {
      RunSocketCallback(callback_, nullptr, result);
      delete this;
      return;
    }
    scoped_ptr<net::StreamSocket> socket(
        new net::TCPClientSocket(address_list_, NULL, net::NetLog::Source()));
    socket->Connect(
        base::Bind(&RunSocketCallback, callback_, base::Passed(&socket)));
    delete this;
  }

  scoped_ptr<net::HostResolver> host_resolver_;
  net::AddressList address_list_;
  AdbClientSocket::SocketCallback callback_;
};

}  // namespace

scoped_refptr<TCPDeviceProvider> TCPDeviceProvider::CreateForLocalhost(
    uint16_t port) {
  TCPDeviceProvider::HostPortSet targets;
  targets.insert(net::HostPortPair("127.0.0.1", port));
  return new TCPDeviceProvider(targets);
}

TCPDeviceProvider::TCPDeviceProvider(const HostPortSet& targets)
    : targets_(targets) {
}

void TCPDeviceProvider::QueryDevices(const SerialsCallback& callback) {
  std::vector<std::string> result;
  for (const net::HostPortPair& target : targets_) {
    const std::string& host = target.host();
    if (std::find(result.begin(), result.end(), host) != result.end())
      continue;
    result.push_back(host);
  }
  callback.Run(result);
}

void TCPDeviceProvider::QueryDeviceInfo(const std::string& serial,
                                        const DeviceInfoCallback& callback) {
  AndroidDeviceManager::DeviceInfo device_info;
  device_info.model = kDeviceModel;
  device_info.connected = true;

  for (const net::HostPortPair& target : targets_) {
    if (serial != target.host())
      continue;
    AndroidDeviceManager::BrowserInfo browser_info;
    browser_info.socket_name = base::UintToString(target.port());
    browser_info.display_name = kBrowserName;
    browser_info.type = AndroidDeviceManager::BrowserInfo::kTypeChrome;

    device_info.browser_info.push_back(browser_info);
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, device_info));
}

void TCPDeviceProvider::OpenSocket(const std::string& serial,
                                   const std::string& socket_name,
                                   const SocketCallback& callback) {
  // Use plain socket for remote debugging and port forwarding on Desktop
  // (debugging purposes).
  int port;
  base::StringToInt(socket_name, &port);
  net::HostPortPair host_port(serial, port);
  new ResolveHostAndOpenSocket(host_port, callback);
}

void TCPDeviceProvider::ReleaseDevice(const std::string& serial) {
  if (!release_callback_.is_null())
    release_callback_.Run();
}

void TCPDeviceProvider::set_release_callback_for_test(
    const base::Closure& callback) {
  release_callback_ = callback;
}

TCPDeviceProvider::~TCPDeviceProvider() {
}
