// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/sockets_udp/sockets_udp_api.h"

namespace extensions {
class ResumableUDPSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "sockets.udp" sockets from callback on native
// socket instances. There is one instance per profile.
class UDPSocketEventDispatcher
    : public ProfileKeyedAPI,
      public base::SupportsWeakPtr<UDPSocketEventDispatcher> {
 public:
  explicit UDPSocketEventDispatcher(Profile* profile);
  virtual ~UDPSocketEventDispatcher();

  // Socket is active, start receving from it.
  void OnSocketBind(const std::string& extension_id, int socket_id);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<UDPSocketEventDispatcher>* GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static UDPSocketEventDispatcher* Get(Profile* profile);

 private:
  friend class ProfileKeyedAPIFactory<UDPSocketEventDispatcher>;

  ResumableUDPSocket* GetUdpSocket(const std::string& extension_id,
                                   int socket_id);

  // Start a receive and register a callback.
  void StartReceive(const std::string& extension_id, int socket_id);

  // Called when socket receive data.
  void ReceiveCallback(const std::string& extension_id,
                       const int socket_id,
                       int bytes_read,
                       scoped_refptr<net::IOBuffer> io_buffer,
                       const std::string& address,
                       int port);

  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "UDPSocketEventDispatcher";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;

  // Usually IO thread (except for unit testing).
  content::BrowserThread::ID  thread_id_;
  Profile* const profile_;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_
