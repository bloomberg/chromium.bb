// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_HTTP_SERVER_IMPL_H_
#define MOJO_SERVICES_NETWORK_HTTP_SERVER_IMPL_H_

#include <map>

#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/network/public/interfaces/http_server.mojom.h"
#include "mojo/services/network/public/interfaces/net_address.mojom.h"
#include "net/server/http_server.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace net {
class HttpServer;
}

namespace mojo {

class HttpConnectionImpl;

class HttpServerImpl : public net::HttpServer::Delegate,
                       public ErrorHandler {
 public:
  static void Create(
      NetAddressPtr local_address,
      HttpServerDelegatePtr delegate,
      const Callback<void(NetworkErrorPtr, NetAddressPtr)>& callback);

  net::HttpServer* server() { return server_.get(); }

 private:
  // The lifetime of the returned HttpServerImpl object is bound to that of
  // |delegate|'s underlying pipe. The object will self-destruct when it is
  // notified that |delegate|'s pipe is closed. Deleting the object directly
  // before that is okay, too.
  explicit HttpServerImpl(HttpServerDelegatePtr delegate);
  ~HttpServerImpl() override;

  int Start(NetAddressPtr local_address);
  NetAddressPtr GetLocalAddress() const;

  // net::HttpServer::Delegate implementation.
  void OnConnect(int connection_id) override;
  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override;
  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override;
  void OnWebSocketMessage(int connection_id, const std::string& data) override;
  void OnClose(int connection_id) override;

  // ErrorHandler implementation.
  void OnConnectionError() override;

  HttpServerDelegatePtr delegate_;
  scoped_ptr<net::HttpServer> server_;

  std::map<int, linked_ptr<HttpConnectionImpl>> connections_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerImpl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_HTTP_SERVER_IMPL_H_
