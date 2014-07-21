// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_SERVICE_IMPL_H_
#define DEVICE_SERIAL_SERIAL_SERVICE_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_connection_factory.h"
#include "device/serial/serial_device_enumerator.h"
#include "mojo/public/cpp/bindings/interface_impl.h"

namespace device {

class SerialServiceImpl : public mojo::InterfaceImpl<serial::SerialService> {
 public:
  explicit SerialServiceImpl(
      scoped_refptr<SerialConnectionFactory> connection_factory);
  SerialServiceImpl(scoped_refptr<SerialConnectionFactory> connection_factory,
                    scoped_ptr<SerialDeviceEnumerator> device_enumerator);
  virtual ~SerialServiceImpl();

  static void Create(scoped_refptr<base::MessageLoopProxy> io_message_loop,
                     mojo::InterfaceRequest<serial::SerialService> request);
  static void CreateOnMessageLoop(
      scoped_refptr<base::MessageLoopProxy> message_loop,
      scoped_refptr<base::MessageLoopProxy> io_message_loop,
      mojo::InterfaceRequest<serial::SerialService> request);

  // mojo::InterfaceImpl<SerialService> overrides.
  virtual void GetDevices(const mojo::Callback<
      void(mojo::Array<serial::DeviceInfoPtr>)>& callback) OVERRIDE;
  virtual void Connect(
      const mojo::String& path,
      serial::ConnectionOptionsPtr options,
      mojo::InterfaceRequest<serial::Connection> connection_request) OVERRIDE;
  virtual void OnConnectionError() OVERRIDE;

 private:
  SerialDeviceEnumerator* GetDeviceEnumerator();
  bool IsValidPath(const mojo::String& path);
  void OnConnected(
      const mojo::Callback<void(serial::ConnectionInfoPtr)>& callback,
      serial::ConnectionInfoPtr result);

  scoped_ptr<SerialDeviceEnumerator> device_enumerator_;
  scoped_refptr<SerialConnectionFactory> connection_factory_;

  DISALLOW_COPY_AND_ASSIGN(SerialServiceImpl);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_SERVICE_IMPL_H_
