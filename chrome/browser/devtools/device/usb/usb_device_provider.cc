// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/device/usb/usb_device_provider.h"

#include "base/strings/stringprintf.h"
#include "chrome/browser/devtools/device/adb/adb_device_info_query.h"
#include "chrome/browser/devtools/device/usb/android_rsa.h"
#include "chrome/browser/devtools/device/usb/android_usb_device.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace {

const char kLocalAbstractCommand[] = "localabstract:%s";

const int kBufferSize = 16 * 1024;

class UsbDeviceImpl : public AndroidDeviceManager::Device {
 public:
  explicit UsbDeviceImpl(AndroidUsbDevice* device);

  virtual void QueryDeviceInfo(const DeviceInfoCallback& callback) OVERRIDE;

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  void OnOpenSocket(const SocketCallback& callback,
                    net::StreamSocket* socket,
                    int result);
  void RunCommand(const std::string& command,
                  const CommandCallback& callback);
  void OpenedForCommand(const CommandCallback& callback,
                        net::StreamSocket* socket,
                        int result);
  void OnRead(net::StreamSocket* socket,
              scoped_refptr<net::IOBuffer> buffer,
              const std::string& data,
              const CommandCallback& callback,
              int result);

  virtual ~UsbDeviceImpl() {}
  scoped_refptr<AndroidUsbDevice> device_;
};


UsbDeviceImpl::UsbDeviceImpl(AndroidUsbDevice* device)
    : Device(device->serial(), device->is_connected()),
      device_(device) {
  device_->InitOnCallerThread();
}

void UsbDeviceImpl::QueryDeviceInfo(const DeviceInfoCallback& callback) {
  AdbDeviceInfoQuery::Start(
      base::Bind(&UsbDeviceImpl::RunCommand, this), callback);
}

void UsbDeviceImpl::OpenSocket(const std::string& name,
                               const SocketCallback& callback) {
  DCHECK(CalledOnValidThread());
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  net::StreamSocket* socket = device_->CreateSocket(socket_name);
  if (!socket) {
    callback.Run(net::ERR_CONNECTION_FAILED, NULL);
    return;
  }
  int result = socket->Connect(base::Bind(&UsbDeviceImpl::OnOpenSocket, this,
                                          callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, NULL);
}

void UsbDeviceImpl::OnOpenSocket(const SocketCallback& callback,
                  net::StreamSocket* socket,
                  int result) {
  callback.Run(result, result == net::OK ? socket : NULL);
}

void UsbDeviceImpl::RunCommand(const std::string& command,
                               const CommandCallback& callback) {
  DCHECK(CalledOnValidThread());
  net::StreamSocket* socket = device_->CreateSocket(command);
  if (!socket) {
    callback.Run(net::ERR_CONNECTION_FAILED, std::string());
    return;
  }
  int result = socket->Connect(base::Bind(&UsbDeviceImpl::OpenedForCommand,
                                          this, callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, std::string());
}

void UsbDeviceImpl::OpenedForCommand(const CommandCallback& callback,
                                     net::StreamSocket* socket,
                                     int result) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);
  result = socket->Read(buffer, kBufferSize,
                        base::Bind(&UsbDeviceImpl::OnRead, this,
                                   socket, buffer, std::string(), callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, std::string(), callback, result);
}

void UsbDeviceImpl::OnRead(net::StreamSocket* socket,
                           scoped_refptr<net::IOBuffer> buffer,
                           const std::string& data,
                           const CommandCallback& callback,
                           int result) {
  if (result <= 0) {
    callback.Run(result, result == 0 ? data : std::string());
    delete socket;
    return;
  }

  std::string new_data = data + std::string(buffer->data(), result);
  result = socket->Read(buffer, kBufferSize,
                        base::Bind(&UsbDeviceImpl::OnRead, this,
                                   socket, buffer, new_data, callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, new_data, callback, result);
}

static void EnumeratedDevices(
    const UsbDeviceProvider::QueryDevicesCallback& callback,
    const AndroidUsbDevices& devices) {
  AndroidDeviceManager::Devices result;
  for (AndroidUsbDevices::const_iterator it = devices.begin();
      it != devices.end(); ++it)
    result.push_back(new UsbDeviceImpl(*it));
  callback.Run(result);
}

} // namespace

// static
void UsbDeviceProvider::CountDevices(
    const base::Callback<void(int)>& callback) {
  AndroidUsbDevice::CountDevices(callback);
}

UsbDeviceProvider::UsbDeviceProvider(Profile* profile){
  rsa_key_.reset(AndroidRSAPrivateKey(profile));
}

UsbDeviceProvider::~UsbDeviceProvider() {
}

void UsbDeviceProvider::QueryDevices(const QueryDevicesCallback& callback) {
  AndroidUsbDevice::Enumerate(
      rsa_key_.get(), base::Bind(&EnumeratedDevices, callback));
}
