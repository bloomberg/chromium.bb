// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sockets_udp/udp_socket_event_dispatcher.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/socket/udp_socket.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {

using content::BrowserThread;

static base::LazyInstance<ProfileKeyedAPIFactory<UDPSocketEventDispatcher> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<UDPSocketEventDispatcher>*
    UDPSocketEventDispatcher::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
UDPSocketEventDispatcher* UDPSocketEventDispatcher::Get(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return ProfileKeyedAPIFactory<UDPSocketEventDispatcher>::GetForProfile(
      profile);
}

UDPSocketEventDispatcher::UDPSocketEventDispatcher(Profile* profile)
    : thread_id_(Socket::kThreadId),
      profile_(profile) {
  ApiResourceManager<ResumableUDPSocket>* manager =
      ApiResourceManager<ResumableUDPSocket>::Get(profile);
  DCHECK(manager) << "There is no socket manager. "
    "If this assertion is failing during a test, then it is likely that "
    "TestExtensionSystem is failing to provide an instance of "
    "ApiResourceManager<ResumableUDPSocket>.";
  sockets_ = manager->data_;
}

UDPSocketEventDispatcher::~UDPSocketEventDispatcher() {}

UDPSocketEventDispatcher::ReceiveParams::ReceiveParams() {}

UDPSocketEventDispatcher::ReceiveParams::~ReceiveParams() {}

void UDPSocketEventDispatcher::OnSocketBind(const std::string& extension_id,
                                            int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  ReceiveParams params;
  params.thread_id = thread_id_;
  params.profile_id = profile_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  StartReceive(params);
}

/* static */
void UDPSocketEventDispatcher::StartReceive(const ReceiveParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  ResumableUDPSocket* socket =
      params.sockets->Get(params.extension_id, params.socket_id);
  if (socket == NULL) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(params.extension_id == socket->owner_extension_id())
    << "Socket has wrong owner.";

  int buffer_size = (socket->buffer_size() <= 0 ? 4096 : socket->buffer_size());
  socket->RecvFrom(buffer_size,
                   base::Bind(&UDPSocketEventDispatcher::ReceiveCallback,
                   params));
}

/* static */
void UDPSocketEventDispatcher::ReceiveCallback(
    const ReceiveParams& params,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer,
    const std::string& address,
    int port) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  // Note: if "bytes_read" < 0, there was a network error, and "bytes_read" is
  // a value from "net::ERR_".

  if (bytes_read >= 0) {
    // Dispatch "onReceive" event.
    sockets_udp::ReceiveInfo receive_info;
    receive_info.socket_id = params.socket_id;
    receive_info.data = std::string(io_buffer->data(), bytes_read);
    receive_info.remote_address = address;
    receive_info.remote_port = port;
    scoped_ptr<base::ListValue> args =
        sockets_udp::OnReceive::Create(receive_info);
    scoped_ptr<Event> event(
      new Event(sockets_udp::OnReceive::kEventName, args.Pass()));
    PostEvent(params, event.Pass());

    // Post a task to delay the read until the socket is available, as
    // calling StartReceive at this point would error with ERR_IO_PENDING.
    BrowserThread::PostTask(
        params.thread_id, FROM_HERE,
        base::Bind(&UDPSocketEventDispatcher::StartReceive, params));
  } else {
    // Dispatch "onReceiveError" event but don't start another read to avoid
    // potential infinite reads if we have a persistent network error.
    sockets_udp::ReceiveErrorInfo receive_error_info;
    receive_error_info.socket_id = params.socket_id;
    receive_error_info.result = bytes_read;
    scoped_ptr<base::ListValue> args =
        sockets_udp::OnReceiveError::Create(receive_error_info);
    scoped_ptr<Event> event(
      new Event(sockets_udp::OnReceiveError::kEventName, args.Pass()));
    PostEvent(params, event.Pass());
  }
}

/* static */
void UDPSocketEventDispatcher::PostEvent(const ReceiveParams& params,
                                         scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DispatchEvent,
                  params.profile_id,
                  params.extension_id,
                  base::Passed(event.Pass())));
}

/*static*/
void UDPSocketEventDispatcher::DispatchEvent(void* profile_id,
                                             const std::string& extension_id,
                                             scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return;

  EventRouter* router = ExtensionSystem::Get(profile)->event_router();
  if (router)
    router->DispatchEventToExtension(extension_id, event.Pass());
}

}  // namespace api
}  // namespace extensions
