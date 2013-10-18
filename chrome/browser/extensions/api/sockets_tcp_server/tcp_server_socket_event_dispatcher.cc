// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sockets_tcp_server/tcp_server_socket_event_dispatcher.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/socket/tcp_socket.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "net/base/net_errors.h"

namespace extensions {
namespace api {

using content::BrowserThread;

static
    base::LazyInstance<ProfileKeyedAPIFactory<TCPServerSocketEventDispatcher> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<TCPServerSocketEventDispatcher>*
    TCPServerSocketEventDispatcher::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
TCPServerSocketEventDispatcher* TCPServerSocketEventDispatcher::Get(
    Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return ProfileKeyedAPIFactory<TCPServerSocketEventDispatcher>::GetForProfile(
      profile);
}

TCPServerSocketEventDispatcher::TCPServerSocketEventDispatcher(Profile* profile)
    : thread_id_(Socket::kThreadId),
      profile_(profile) {
  ApiResourceManager<ResumableTCPServerSocket>* server_manager =
      ApiResourceManager<ResumableTCPServerSocket>::Get(profile);
  DCHECK(server_manager) << "There is no server socket manager. "
    "If this assertion is failing during a test, then it is likely that "
    "TestExtensionSystem is failing to provide an instance of "
    "ApiResourceManager<ResumableTCPServerSocket>.";
  server_sockets_ = server_manager->data_;

  ApiResourceManager<ResumableTCPSocket>* client_manager =
      ApiResourceManager<ResumableTCPSocket>::Get(profile);
  DCHECK(client_manager) << "There is no client socket manager. "
    "If this assertion is failing during a test, then it is likely that "
    "TestExtensionSystem is failing to provide an instance of "
    "ApiResourceManager<ResumableTCPSocket>.";
  client_sockets_ = client_manager->data_;
}

TCPServerSocketEventDispatcher::~TCPServerSocketEventDispatcher() {}

TCPServerSocketEventDispatcher::AcceptParams::AcceptParams() {}

TCPServerSocketEventDispatcher::AcceptParams::~AcceptParams() {}

void TCPServerSocketEventDispatcher::OnServerSocketListen(
    const std::string& extension_id,
    int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  StartSocketAccept(extension_id, socket_id);
}

void TCPServerSocketEventDispatcher::OnServerSocketResume(
    const std::string& extension_id,
    int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  StartSocketAccept(extension_id, socket_id);
}

void TCPServerSocketEventDispatcher::StartSocketAccept(
    const std::string& extension_id,
    int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  AcceptParams params;
  params.thread_id = thread_id_;
  params.profile_id = profile_;
  params.extension_id = extension_id;
  params.server_sockets = server_sockets_;
  params.client_sockets = client_sockets_;
  params.socket_id = socket_id;

  StartAccept(params);
}

// static
void TCPServerSocketEventDispatcher::StartAccept(const AcceptParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  ResumableTCPServerSocket* socket =
      params.server_sockets->Get(params.extension_id, params.socket_id);
  if (!socket) {
    // This can happen if the socket is closed while our callback is active.
    return;
  }
  DCHECK(params.extension_id == socket->owner_extension_id())
    << "Socket has wrong owner.";

  // Don't start another accept if the socket has been paused.
  if (socket->paused())
    return;

  socket->Accept(base::Bind(&TCPServerSocketEventDispatcher::AcceptCallback,
                 params));
}

// static
void TCPServerSocketEventDispatcher::AcceptCallback(
    const AcceptParams& params,
    int result_code,
    net::TCPClientSocket *socket) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  if (result_code >= 0) {
    ResumableTCPSocket *client_socket =
        new ResumableTCPSocket(socket, params.extension_id, true);
    client_socket->set_paused(true);
    int client_socket_id = params.client_sockets->Add(client_socket);

    // Dispatch "onAccept" event.
    sockets_tcp_server::AcceptInfo accept_info;
    accept_info.socket_id = params.socket_id;
    accept_info.client_socket_id = client_socket_id;
    scoped_ptr<base::ListValue> args =
        sockets_tcp_server::OnAccept::Create(accept_info);
    scoped_ptr<Event> event(
      new Event(sockets_tcp_server::OnAccept::kEventName, args.Pass()));
    PostEvent(params, event.Pass());

    // Post a task to delay the "accept" until the socket is available, as
    // calling StartAccept at this point would error with ERR_IO_PENDING.
    BrowserThread::PostTask(
        params.thread_id, FROM_HERE,
        base::Bind(&TCPServerSocketEventDispatcher::StartAccept, params));
  } else {
    // Dispatch "onAcceptError" event but don't start another accept to avoid
    // potential infinite "accepts" if we have a persistent network error.
    sockets_tcp_server::AcceptErrorInfo accept_error_info;
    accept_error_info.socket_id = params.socket_id;
    accept_error_info.result_code = result_code;
    scoped_ptr<base::ListValue> args =
        sockets_tcp_server::OnAcceptError::Create(accept_error_info);
    scoped_ptr<Event> event(
      new Event(sockets_tcp_server::OnAcceptError::kEventName, args.Pass()));
    PostEvent(params, event.Pass());

    // Since we got an error, the socket is now "paused" until the application
    // "resumes" it.
    ResumableTCPServerSocket* socket =
        params.server_sockets->Get(params.extension_id, params.socket_id);
    if (socket) {
      socket->set_paused(true);
    }
  }
}

// static
void TCPServerSocketEventDispatcher::PostEvent(const AcceptParams& params,
                                               scoped_ptr<Event> event) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DispatchEvent,
                 params.profile_id,
                 params.extension_id,
                 base::Passed(event.Pass())));
}

// static
void TCPServerSocketEventDispatcher::DispatchEvent(
    void* profile_id,
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
