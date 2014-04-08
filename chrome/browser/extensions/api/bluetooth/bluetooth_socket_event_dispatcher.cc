// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth/bluetooth_socket_event_dispatcher.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"
#include "chrome/common/extensions/api/bluetooth.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace {

namespace bluetooth = extensions::api::bluetooth;
using extensions::BluetoothApiSocket;

int kDefaultBufferSize = 4096;

bluetooth::ReceiveError MapErrorReason(BluetoothApiSocket::ErrorReason value) {
  switch (value) {
    case BluetoothApiSocket::kDisconnected:
      return bluetooth::RECEIVE_ERROR_DISCONNECTED;
    case BluetoothApiSocket::kIOPending:
    // kIOPending is not relevant to apps, as BluetoothSocketEventDispatcher
    // handles this specific error.
    // fallthrough
    default:
      return bluetooth::RECEIVE_ERROR_SYSTEM_ERROR;
  }
}

}  // namespace

namespace extensions {
namespace api {

using content::BrowserThread;

BluetoothSocketEventDispatcher::BluetoothSocketEventDispatcher(
    content::BrowserContext* context,
    scoped_refptr<SocketData> socket_data)
    : thread_id_(BluetoothApiSocket::kThreadId),
      browser_context_id_(context),
      socket_data_(socket_data) {}

BluetoothSocketEventDispatcher::~BluetoothSocketEventDispatcher() {}

BluetoothSocketEventDispatcher::ReceiveParams::ReceiveParams() {}

BluetoothSocketEventDispatcher::ReceiveParams::~ReceiveParams() {}

void BluetoothSocketEventDispatcher::OnSocketResume(
    const std::string& extension_id,
    int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  ReceiveParams params;
  params.extension_id = extension_id;
  params.socket_id = socket_id;
  StartReceive(params);
}

void BluetoothSocketEventDispatcher::StartReceive(const ReceiveParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  BluetoothApiSocket* socket =
      socket_data_->Get(params.extension_id, params.socket_id);
  if (!socket) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(params.extension_id == socket->owner_extension_id())
      << "Socket has wrong owner.";

  // Don't start another read if the socket has been paused.
  if (socket->paused())
    return;

  int buffer_size = socket->buffer_size();
  if (buffer_size <= 0)
    buffer_size = kDefaultBufferSize;
  socket->Receive(
      buffer_size,
      base::Bind(
          &BluetoothSocketEventDispatcher::ReceiveCallback, this, params),
      base::Bind(
          &BluetoothSocketEventDispatcher::ReceiveErrorCallback, this, params));
}

void BluetoothSocketEventDispatcher::ReceiveCallback(
    const ReceiveParams& params,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  // Dispatch "onReceive" event.
  bluetooth::ReceiveInfo receive_info;
  receive_info.socket_id = params.socket_id;
  receive_info.data = std::string(io_buffer->data(), bytes_read);
  scoped_ptr<base::ListValue> args = bluetooth::OnReceive::Create(receive_info);
  scoped_ptr<Event> event(
      new Event(bluetooth::OnReceive::kEventName, args.Pass()));
  PostEvent(params, event.Pass());

  // Post a task to delay the read until the socket is available, as
  // calling StartReceive at this point would error with ERR_IO_PENDING.
  BrowserThread::PostTask(
      thread_id_,
      FROM_HERE,
      base::Bind(&BluetoothSocketEventDispatcher::StartReceive, this, params));
}

void BluetoothSocketEventDispatcher::ReceiveErrorCallback(
    const ReceiveParams& params,
    BluetoothApiSocket::ErrorReason error_reason,
    const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  if (error_reason == BluetoothApiSocket::kIOPending) {
    // This happens when resuming a socket which already had an active "read"
    // callback. We can safely ignore this error, as the application should not
    // care.
    return;
  }

  // Dispatch "onReceiveError" event but don't start another read to avoid
  // potential infinite reads if we have a persistent network error.
  bluetooth::ReceiveErrorInfo receive_error_info;
  receive_error_info.socket_id = params.socket_id;
  receive_error_info.error_message = error;
  receive_error_info.error = MapErrorReason(error_reason);
  scoped_ptr<base::ListValue> args =
      bluetooth::OnReceiveError::Create(receive_error_info);
  scoped_ptr<Event> event(
      new Event(bluetooth::OnReceiveError::kEventName, args.Pass()));
  PostEvent(params, event.Pass());

  // Since we got an error, the socket is now "paused" until the application
  // "resumes" it.
  BluetoothApiSocket* socket =
      socket_data_->Get(params.extension_id, params.socket_id);
  if (socket) {
    socket->set_paused(true);
  }
}

void BluetoothSocketEventDispatcher::PostEvent(const ReceiveParams& params,
                                               scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&BluetoothSocketEventDispatcher::DispatchEvent,
                 this,
                 params.extension_id,
                 base::Passed(event.Pass())));
}

void BluetoothSocketEventDispatcher::DispatchEvent(
    const std::string& extension_id,
    scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id_);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  EventRouter* router = ExtensionSystem::Get(context)->event_router();
  if (router)
    router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace api
}  // namespace extensions
