// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sockets_udp/udp_socket_event_dispatcher.h"

#include "chrome/browser/extensions/api/socket/udp_socket.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {

static base::LazyInstance<ProfileKeyedAPIFactory<UDPSocketEventDispatcher> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<UDPSocketEventDispatcher>*
    UDPSocketEventDispatcher::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
UDPSocketEventDispatcher* UDPSocketEventDispatcher::Get(Profile* profile) {
  return ProfileKeyedAPIFactory<UDPSocketEventDispatcher>::GetForProfile(
      profile);
}

UDPSocketEventDispatcher::UDPSocketEventDispatcher(Profile* profile)
    : thread_id_(Socket::kThreadId),
      profile_(profile) {
}

UDPSocketEventDispatcher::~UDPSocketEventDispatcher() {
}

ResumableUDPSocket* UDPSocketEventDispatcher::GetUdpSocket(
    const std::string& extension_id,
    int socket_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(thread_id_));

  ApiResourceManager<ResumableUDPSocket>* manager =
    ApiResourceManager<ResumableUDPSocket>::Get(profile_);
  DCHECK(manager) << "There is no socket manager. "
    "If this assertion is failing during a test, then it is likely that "
    "TestExtensionSystem is failing to provide an instance of "
    "ApiResourceManager<ResumableUDPSocket>.";

  return manager->Get(extension_id, socket_id);
}

void UDPSocketEventDispatcher::OnSocketBind(const std::string& extension_id,
                                         int socket_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(thread_id_));
  StartReceive(extension_id, socket_id);
}

void UDPSocketEventDispatcher::StartReceive(const std::string& extension_id,
                                         int socket_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(thread_id_));
  ResumableUDPSocket* socket = GetUdpSocket(extension_id, socket_id);
  if (socket == NULL) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(extension_id == socket->owner_extension_id())
    << "Socket has wrong owner.";

  int buffer_size = (socket->buffer_size() <= 0 ? 4096 : socket->buffer_size());
  socket->RecvFrom(buffer_size,
                   base::Bind(&UDPSocketEventDispatcher::ReceiveCallback,
                              AsWeakPtr(),
                              extension_id,
                              socket_id));
}

void UDPSocketEventDispatcher::ReceiveCallback(
    const std::string& extension_id,
    const int socket_id,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer,
    const std::string& address,
    int port) {
  DCHECK(content::BrowserThread::CurrentlyOn(thread_id_));

  // Note: if "bytes_read" < 0, there was a network error, and "bytes_read" is
  // a value from "net::ERR_".

  if (bytes_read >= 0) {
    // Dispatch event.
    sockets_udp::ReceiveInfo receive_info;
    receive_info.socket_id = socket_id;
    receive_info.data = std::string(io_buffer->data(), bytes_read);
    receive_info.remote_address = address;
    receive_info.remote_port = port;
    scoped_ptr<base::ListValue> args =
        sockets_udp::OnReceive::Create(receive_info);
    scoped_ptr<Event> event(
      new Event(sockets_udp::OnReceive::kEventName, args.Pass()));
    ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
        extension_id, event.Pass());

    // Post a task to delay the read until the socket is available, as
    // calling StartReceive at this point would error with ERR_IO_PENDING.
    content::BrowserThread::PostTask(thread_id_, FROM_HERE,
        base::Bind(&UDPSocketEventDispatcher::StartReceive,
                    AsWeakPtr(), extension_id, socket_id));
  } else {
    // Dispatch event but don't start another read to avoid infinite read if
    // we have a persistent network error.
    sockets_udp::ReceiveErrorInfo receive_error_info;
    receive_error_info.socket_id = socket_id;
    receive_error_info.result = bytes_read;
    scoped_ptr<base::ListValue> args =
        sockets_udp::OnReceiveError::Create(receive_error_info);
    scoped_ptr<Event> event(
      new Event(sockets_udp::OnReceiveError::kEventName, args.Pass()));
    ExtensionSystem::Get(profile_)->event_router()->DispatchEventToExtension(
        extension_id, event.Pass());
  }
}

}  // namespace api
}  // namespace extensions
