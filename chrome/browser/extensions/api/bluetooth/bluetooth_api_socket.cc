// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"

#include "device/bluetooth/bluetooth_socket.h"
#include "net/base/io_buffer.h"

namespace extensions {

// static
static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ApiResourceManager<BluetoothApiSocket> > >
    g_server_factory = LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<BluetoothApiSocket> >*
ApiResourceManager<BluetoothApiSocket>::GetFactoryInstance() {
  return g_server_factory.Pointer();
}

BluetoothApiSocket::BluetoothApiSocket(
    const std::string& owner_extension_id,
    scoped_refptr<device::BluetoothSocket> socket,
    const std::string& device_address,
    const device::BluetoothUUID& uuid)
    : ApiResource(owner_extension_id),
      socket_(socket),
      device_address_(device_address),
      uuid_(uuid),
      persistent_(false),
      buffer_size_(0),
      paused_(true) {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
}

BluetoothApiSocket::~BluetoothApiSocket() {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
  socket_->Close();
}

void BluetoothApiSocket::Disconnect(const base::Closure& success_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
  socket_->Disconnect(success_callback);
}

bool BluetoothApiSocket::IsPersistent() const {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
  return persistent_;
}

void BluetoothApiSocket::Receive(
    int count,
    const ReceiveCompletionCallback& success_callback,
    const ReceiveErrorCompletionCallback& error_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
  socket_->Receive(count,
                   success_callback,
                   base::Bind(&OnSocketReceiveError, error_callback));
}

// static
void BluetoothApiSocket::OnSocketReceiveError(
    const ReceiveErrorCompletionCallback& error_callback,
    device::BluetoothSocket::ErrorReason reason,
    const std::string& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
  BluetoothApiSocket::ErrorReason error_reason;
  switch (reason) {
    case device::BluetoothSocket::kIOPending:
      error_reason = BluetoothApiSocket::kIOPending;
      break;
    case device::BluetoothSocket::kDisconnected:
      error_reason = BluetoothApiSocket::kDisconnected;
      break;
    case device::BluetoothSocket::kSystemError:
      error_reason = BluetoothApiSocket::kSystemError;
      break;
    default:
      NOTREACHED();
  }
  error_callback.Run(error_reason, message);
}

void BluetoothApiSocket::Send(scoped_refptr<net::IOBuffer> buffer,
                              int buffer_size,
                              const SendCompletionCallback& success_callback,
                              const ErrorCompletionCallback& error_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(kThreadId));
  socket_->Send(buffer, buffer_size, success_callback, error_callback);
}

}  // namespace extensions
