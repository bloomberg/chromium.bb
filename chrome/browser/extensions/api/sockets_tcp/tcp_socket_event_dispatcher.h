// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SOCKETS_TCP_TCP_SOCKET_EVENT_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SOCKETS_TCP_TCP_SOCKET_EVENT_DISPATCHER_H_

#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/browser/extensions/api/sockets_tcp/sockets_tcp_api.h"

namespace extensions {
struct Event;
class ResumableTCPSocket;
}

namespace extensions {
namespace api {

// Dispatch events related to "sockets.tcp" sockets from callback on native
// socket instances. There is one instance per profile.
class TCPSocketEventDispatcher
    : public ProfileKeyedAPI,
      public base::SupportsWeakPtr<TCPSocketEventDispatcher> {
 public:
  explicit TCPSocketEventDispatcher(Profile* profile);
  virtual ~TCPSocketEventDispatcher();

  // Socket is active, start receving from it.
  void OnSocketConnect(const std::string& extension_id, int socket_id);

  // Socket is active again, start receiving data from it.
  void OnSocketResume(const std::string& extension_id, int socket_id);

  // ProfileKeyedAPI implementation.
  static ProfileKeyedAPIFactory<TCPSocketEventDispatcher>* GetFactoryInstance();

  // Convenience method to get the SocketEventDispatcher for a profile.
  static TCPSocketEventDispatcher* Get(Profile* profile);

 private:
  typedef ApiResourceManager<ResumableTCPSocket>::ApiResourceData SocketData;
  friend class ProfileKeyedAPIFactory<TCPSocketEventDispatcher>;
  // ProfileKeyedAPI implementation.
  static const char* service_name() {
    return "TCPSocketEventDispatcher";
  }
  static const bool kServiceHasOwnInstanceInIncognito = true;
  static const bool kServiceIsNULLWhileTesting = true;

  // base::Bind supports methods with up to 6 parameters. ReadParams is used
  // as a workaround that limitation for invoking StartReceive.
  struct ReadParams {
    ReadParams();
    ~ReadParams();

    content::BrowserThread::ID thread_id;
    void* profile_id;
    std::string extension_id;
    scoped_refptr<SocketData> sockets;
    int socket_id;
  };

  // Start a receive and register a callback.
  void StartSocketRead(const std::string& extension_id, int socket_id);

  // Start a receive and register a callback.
  static void StartRead(const ReadParams& params);

  // Called when socket receive data.
  static void ReadCallback(const ReadParams& params,
                           int bytes_read,
                           scoped_refptr<net::IOBuffer> io_buffer);

  // Post an extension event from IO to UI thread
  static void PostEvent(const ReadParams& params, scoped_ptr<Event> event);

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

#endif  // CHROME_BROWSER_EXTENSIONS_API_SOCKETS_TCP_TCP_SOCKET_EVENT_DISPATCHER_H_
