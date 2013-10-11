// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sockets_tcp/tcp_socket_event_dispatcher.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/socket/tcp_socket.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "net/base/net_errors.h"

namespace {
int kDefaultBufferSize = 4096;
}

namespace extensions {
namespace api {

using content::BrowserThread;

static base::LazyInstance<ProfileKeyedAPIFactory<TCPSocketEventDispatcher> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<TCPSocketEventDispatcher>*
    TCPSocketEventDispatcher::GetFactoryInstance() {
  return &g_factory.Get();
}

// static
TCPSocketEventDispatcher* TCPSocketEventDispatcher::Get(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return ProfileKeyedAPIFactory<TCPSocketEventDispatcher>::GetForProfile(
      profile);
}

TCPSocketEventDispatcher::TCPSocketEventDispatcher(Profile* profile)
    : thread_id_(Socket::kThreadId),
      profile_(profile) {
  ApiResourceManager<ResumableTCPSocket>* manager =
      ApiResourceManager<ResumableTCPSocket>::Get(profile);
  DCHECK(manager) << "There is no socket manager. "
    "If this assertion is failing during a test, then it is likely that "
    "TestExtensionSystem is failing to provide an instance of "
    "ApiResourceManager<ResumableTCPSocket>.";
  sockets_ = manager->data_;
}

TCPSocketEventDispatcher::~TCPSocketEventDispatcher() {}

TCPSocketEventDispatcher::ReadParams::ReadParams() {}

TCPSocketEventDispatcher::ReadParams::~ReadParams() {}

void TCPSocketEventDispatcher::OnSocketConnect(const std::string& extension_id,
                                               int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  StartSocketRead(extension_id, socket_id);
}

void TCPSocketEventDispatcher::OnSocketResume(const std::string& extension_id,
                                              int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  StartSocketRead(extension_id, socket_id);
}

void TCPSocketEventDispatcher::StartSocketRead(const std::string& extension_id,
                                               int socket_id) {
  DCHECK(BrowserThread::CurrentlyOn(thread_id_));

  ReadParams params;
  params.thread_id = thread_id_;
  params.profile_id = profile_;
  params.extension_id = extension_id;
  params.sockets = sockets_;
  params.socket_id = socket_id;

  StartRead(params);
}

// static
void TCPSocketEventDispatcher::StartRead(const ReadParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  ResumableTCPSocket* socket =
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
  socket->Read(buffer_size,
               base::Bind(&TCPSocketEventDispatcher::ReadCallback,
               params));
}

// static
void TCPSocketEventDispatcher::ReadCallback(
    const ReadParams& params,
    int bytes_read,
    scoped_refptr<net::IOBuffer> io_buffer) {
  DCHECK(BrowserThread::CurrentlyOn(params.thread_id));

  // Note: if "bytes_read" < 0, there was a network error, and "bytes_read" is
  // a value from "net::ERR_".

  if (bytes_read >= 0) {
    // Dispatch "onReceive" event.
    sockets_tcp::ReceiveInfo receive_info;
    receive_info.socket_id = params.socket_id;
    receive_info.data = std::string(io_buffer->data(), bytes_read);
    scoped_ptr<base::ListValue> args =
        sockets_tcp::OnReceive::Create(receive_info);
    scoped_ptr<Event> event(
      new Event(sockets_tcp::OnReceive::kEventName, args.Pass()));
    PostEvent(params, event.Pass());

    // Post a task to delay the read until the socket is available, as
    // calling StartReceive at this point would error with ERR_IO_PENDING.
    BrowserThread::PostTask(
        params.thread_id, FROM_HERE,
        base::Bind(&TCPSocketEventDispatcher::StartRead, params));
  } else if (bytes_read == net::ERR_IO_PENDING) {
    // This happens when resuming a socket which already had an
    // active "read" callback.
  } else {
    // Dispatch "onReceiveError" event but don't start another read to avoid
    // potential infinite reads if we have a persistent network error.
    sockets_tcp::ReceiveErrorInfo receive_error_info;
    receive_error_info.socket_id = params.socket_id;
    receive_error_info.result_code = bytes_read;
    scoped_ptr<base::ListValue> args =
        sockets_tcp::OnReceiveError::Create(receive_error_info);
    scoped_ptr<Event> event(
      new Event(sockets_tcp::OnReceiveError::kEventName, args.Pass()));
    PostEvent(params, event.Pass());

    // Since we got an error, the socket is now "paused" until the application
    // "resumes" it.
    ResumableTCPSocket* socket =
        params.sockets->Get(params.extension_id, params.socket_id);
    if (socket) {
      socket->set_paused(true);
    }
  }
}

// static
void TCPSocketEventDispatcher::PostEvent(const ReadParams& params,
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
void TCPSocketEventDispatcher::DispatchEvent(void* profile_id,
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
