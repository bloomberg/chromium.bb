// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include "base/bind.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner.h"
#include "mojo/services/public/interfaces/network/network_service.mojom.h"

namespace mojo {
namespace examples {

class HttpServerApp : public ApplicationDelegate {
 public:
  virtual void Initialize(ApplicationImpl* app) override {
    app->ConnectToService("mojo:mojo_network_service", &network_service_);
    Start();
  }

 private:
  void OnListeningStarted(NetworkErrorPtr err) {
  }

  void OnSocketBound(NetworkErrorPtr err, NetAddressPtr bound_address) {
    if (err->code != 0) {
      printf("Bound err = %d\n", err->code);
      return;
    }

    printf("Got address = %d.%d.%d.%d:%d\n",
           (int)bound_address->ipv4->addr[0],
           (int)bound_address->ipv4->addr[1],
           (int)bound_address->ipv4->addr[2],
           (int)bound_address->ipv4->addr[3],
           (int)bound_address->ipv4->port);

    bound_socket_->StartListening(
        GetProxy(&server_socket_),
        base::Bind(&HttpServerApp::OnListeningStarted,
                   base::Unretained(this)));
  }

  void Start() {
    NetAddressPtr net_address(NetAddress::New());
    net_address->family = NET_ADDRESS_FAMILY_IPV4;
    net_address->ipv4 = NetAddressIPv4::New();
    net_address->ipv4->addr.resize(4);
    net_address->ipv4->addr[0] = 0;
    net_address->ipv4->addr[1] = 0;
    net_address->ipv4->addr[2] = 0;
    net_address->ipv4->addr[3] = 0;

    network_service_->CreateTCPBoundSocket(
        net_address.Pass(),
        GetProxy(&bound_socket_),
        base::Bind(&HttpServerApp::OnSocketBound, base::Unretained(this)));
  }

  NetworkServicePtr network_service_;
  TCPBoundSocketPtr bound_socket_;
  TCPServerSocketPtr server_socket_;
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::examples::HttpServerApp);
  return runner.Run(shell_handle);
}
