// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bluetooth_socket/bluetooth_socket_event_dispatcher.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/bluetooth/bluetooth_api_socket.h"
#include "chrome/common/extensions/api/bluetooth_socket.h"
#include "extensions/browser/event_router.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace {

namespace bluetooth_socket = extensions::api::bluetooth_socket;
using extensions::BluetoothApiSocket;

int kDefaultBufferSize = 4096;

bluetooth_socket::ReceiveError MapErrorReason(
    BluetoothApiSocket::ErrorReason value) {
  switch (value) {
    case BluetoothApiSocket::kDisconnected:
      return bluetooth_socket::RECEIVE_ERROR_DISCONNECTED;
    case BluetoothApiSocket::kNotConnected:
    // kNotConnected is impossible since a socket has to be connected to be
    // able to call Receive() on it.
    // fallthrough
    case BluetoothApiSocket::kIOPending:
    // kIOPending is not relevant to apps, as BluetoothSocketEventDispatcher
    // handles this specific error.
    // fallthrough
    default:
      return bluetooth_socket::RECEIVE_ERROR_SYSTEM_ERROR;
  }
}

}  // namespace

namespace extensions {
namespace api {

using content::BrowserThread;

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher> > g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher>*
BluetoothSocketEventDispatcher::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
BluetoothSocketEventDispatcher* BluetoothSocketEventDispatcher::Get(
    content::BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return BrowserContextKeyedAPIFactory<BluetoothSocketEventDispatcher>::Get(
      context);
}

BluetoothSocketEventDispatcher::BluetoothSocketEventDispatcher(
    content::BrowserContext* context)
    : thread_id_(BluetoothApiSocket::kThreadId),
      browser_context_(context) {
  ApiResourceManager<BluetoothApiSocket>* manager =
      ApiResourceManager<BluetoothApiSocket>::Get(browser_context_);
  DCHECK(manager)
      << "There is no socket manager. "
         "If this assertion is failing during a test, then it is likely that "
         "TestExtensionSystem is failing to provide an instance of "
         "ApiResourceManager<BluetoothApiSocket>.";
  sockets_ = manager->data_;
}

BluetoothSocketEventDispatcher::~BluetoothSocketEventDispatcher() {}

BluetoothSocketEventDispatcher::ReceiveParams::ReceiveParams() {}

BluetoothSocketEventDispatcher::ReceiveParams::~ReceiveParams() {}

void BluetoothSocketEventDispatcher::OnSocketResume(
    const std::string& extension_id,
    int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  ReceiveParams params;
  params.thread_id = thread_id_;
  params.browser_context_id = browser_context_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  StartReceive(params);
}

// static
void BluetoothSocketEventDispatcher::StartReceive(const ReceiveParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
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
          &BluetoothSocketEventDispatcher::ReceiveCallback, params),
      base::Bind(
          &BluetoothSocketEventDispatcher::ReceiveErrorCallback, params));
}

// static
void BluetoothSocketEventDispatcher::ReceiveCallback(
    const ReceiveParams& params,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  // Dispatch "onReceive" event.
  bluetooth_socket::ReceiveInfo receive_info;
  receive_info.socket_id = params.socket_id;
  receive_info.data = std::string(io_buffer->data(), bytes_read);
  scoped_ptr<base::ListValue> args =
      bluetooth_socket::OnReceive::Create(receive_info);
  scoped_ptr<Event> event(
      new Event(bluetooth_socket::OnReceive::kEventName, args.Pass()));
  PostEvent(params, event.Pass());

  // Post a task to delay the read until the socket is available, as
  // calling StartReceive at this point would error with ERR_IO_PENDING.
  BrowserThread::PostTask(
      params.thread_id,
      FROM_HERE,
      base::Bind(&BluetoothSocketEventDispatcher::StartReceive, params));
}

// static
void BluetoothSocketEventDispatcher::ReceiveErrorCallback(
    const ReceiveParams& params,
    BluetoothApiSocket::ErrorReason error_reason,
    const std::string& error) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  if (error_reason == BluetoothApiSocket::kIOPending) {
    // This happens when resuming a socket which already had an active "read"
    // callback. We can safely ignore this error, as the application should not
    // care.
    return;
  }

  // Dispatch "onReceiveError" event but don't start another read to avoid
  // potential infinite reads if we have a persistent network error.
  bluetooth_socket::ReceiveErrorInfo receive_error_info;
  receive_error_info.socket_id = params.socket_id;
  receive_error_info.error_message = error;
  receive_error_info.error = MapErrorReason(error_reason);
  scoped_ptr<base::ListValue> args =
      bluetooth_socket::OnReceiveError::Create(receive_error_info);
  scoped_ptr<Event> event(
      new Event(bluetooth_socket::OnReceiveError::kEventName, args.Pass()));
  PostEvent(params, event.Pass());

  // Since we got an error, the socket is now "paused" until the application
  // "resumes" it.
  BluetoothApiSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (socket) {
    socket->set_paused(true);
  }
}

// static
void BluetoothSocketEventDispatcher::PostEvent(const ReceiveParams& params,
                                               scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DispatchEvent,
                 params.browser_context_id,
                 params.extension_id,
                 base::Passed(event.Pass())));
}

// static
void BluetoothSocketEventDispatcher::DispatchEvent(
    void* browser_context_id,
    const std::string& extension_id,
    scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  content::BrowserContext* context =
      reinterpret_cast<content::BrowserContext*>(browser_context_id);
  if (!extensions::ExtensionsBrowserClient::Get()->IsValidContext(context))
    return;

  EventRouter* router = EventRouter::Get(context);
  if (router)
    router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace api
}  // namespace extensions
