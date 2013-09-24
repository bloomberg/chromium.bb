// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/sockets_udp/sockets_udp_api.h"

namespace extensions {
struct Event;
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
  typedef ApiResourceManager<ResumableUDPSocket>::ApiResourceData SocketData;
  friend class ProfileKeyedAPIFactory<UDPSocketEventDispatcher>;
  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "UDPSocketEventDispatcher";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. ReceiveParams is used
  // as a workaround that limitation for invoking StartReceive.
  struct ReceiveParams {
    ReceiveParams();
    ~ReceiveParams();

    content::BrowserThread::ID thread_id;
    void* profile_id;
    std::string extension_id;
    scoped_refptr<SocketData> sockets;
    int socket_id;
  };

  // Start a receive and register a callback.
  static void StartReceive(const ReceiveParams& params);

  // Called when socket receive data.
  static void ReceiveCallback(const ReceiveParams& params,
                              int bytes_read,
                              scoped_refptr<net::IOBuffer> io_buffer,
                              const std::string& address,
                              int port);

  // Post an extension event from IO to UI thread
  static void PostEvent(const ReceiveParams& params, scoped_ptr<Event> event);

  // Dispatch an extension event on to EventRouter instance on UI thread.
  static void DispatchEvent(void* profile_id,
                            const std::string& extension_id,
                            scoped_ptr<Event> event);

  // Usually IO thread (except for unit testing).
  content::BrowserThread::ID thread_id_;
  Profile* const profile_;
  scoped_refptr<SocketData> sockets_;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKETS_UDP_UDP_SOCKET_EVENT_DISPATCHER_H_
