// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKETS_TCP_SERVER_TCP_SERVER_SOCKET_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKETS_TCP_SERVER_TCP_SERVER_SOCKET_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/sockets_tcp/sockets_tcp_api.h"
#include "chrome/browser/extensions/api/sockets_tcp_server/sockets_tcp_server_api.h"

namespace extensions {
struct Event;
class ResumableTCPSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "sockets.tcp" sockets from callback on native
// socket instances. There is one instance per profile.
class TCPServerSocketEventDispatcher
    : public ProfileKeyedAPI,
      public base::SupportsWeakPtr<TCPServerSocketEventDispatcher> {
 public:
  explicit TCPServerSocketEventDispatcher(Profile* profile);
  virtual ~TCPServerSocketEventDispatcher();

  // Server socket is active, start accepting connections from it.
  void OnServerSocketListen(const std::string& extension_id, int socket_id);

  // Server socket is active again, start accepting connections from it.
  void OnServerSocketResume(const std::string& extension_id, int socket_id);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<TCPServerSocketEventDispatcher>*
      GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static TCPServerSocketEventDispatcher* Get(Profile* profile);

 private:
  typedef ApiResourceManager<ResumableTCPServerSocket>::ApiResourceData
      ServerSocketData;
  typedef ApiResourceManager<ResumableTCPSocket>::ApiResourceData
      ClientSocketData;
  friend class ProfileKeyedAPIFactory<TCPServerSocketEventDispatcher>;
  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "TCPServerSocketEventDispatcher";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. AcceptParams is used
  // as a workaround that limitation for invoking StartAccept.
  struct AcceptParams {
    AcceptParams();
    ~AcceptParams();

    content::BrowserThread::ID thread_id;
    void* profile_id;
    std::string extension_id;
    scoped_refptr<ServerSocketData> server_sockets;
    scoped_refptr<ClientSocketData> client_sockets;
    int socket_id;
  };

  // Start an accept and register a callback.
  void StartSocketAccept(const std::string& extension_id, int socket_id);

  // Start an accept and register a callback.
  static void StartAccept(const AcceptParams& params);

  // Called when socket accepts a new connection.
  static void AcceptCallback(const AcceptParams& params,
                             int result_code,
                             net::TCPClientSocket *socket);

  // Post an extension event from |thread_id| to UI thread
  static void PostEvent(const AcceptParams& params,
                        scoped_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  static void DispatchEvent(void* profile_id,
                            const std::string& extension_id,
                            scoped_ptr<Event> event);

  // Usually IO thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  Profile* const profile_;
  scoped_refptr<ServerSocketData> server_sockets_;
  scoped_refptr<ClientSocketData> client_sockets_;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKETS_TCP_SERVER_TCP_SERVER_SOCKET_EVENT_DISPATCHER_H_
