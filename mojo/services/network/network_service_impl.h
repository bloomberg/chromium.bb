// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_SERVICE_IMPL_H_
#define MOJO_SERVICES_NETWORK_NETWORK_SERVICE_IMPL_H_

#include "base/compiler_specific.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/shell/public/cpp/shell.h"
#include "url/gurl.h"

namespace mojo {

class NetworkServiceImpl : public NetworkService {
 public:
  NetworkServiceImpl(scoped_ptr<mojo::AppRefCount> app_refcount,
                     InterfaceRequest<NetworkService> request);
  ~NetworkServiceImpl() override;

  // NetworkService methods:
  void CreateTCPBoundSocket(
      NetAddressPtr local_address,
      InterfaceRequest<TCPBoundSocket> bound_socket,
      const CreateTCPBoundSocketCallback& callback) override;
  void CreateTCPConnectedSocket(
      NetAddressPtr remote_address,
      ScopedDataPipeConsumerHandle send_stream,
      ScopedDataPipeProducerHandle receive_stream,
      InterfaceRequest<TCPConnectedSocket> client_socket,
      const CreateTCPConnectedSocketCallback& callback) override;
  void CreateUDPSocket(InterfaceRequest<UDPSocket> socket) override;
  void CreateHttpServer(NetAddressPtr local_address,
                        HttpServerDelegatePtr delegate,
                        const CreateHttpServerCallback& callback) override;
  void GetMimeTypeFromFile(
      const mojo::String& file_path,
      const GetMimeTypeFromFileCallback& callback) override;

 private:
  scoped_ptr<mojo::AppRefCount> app_refcount_;
  StrongBinding<NetworkService> binding_;
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_SERVICE_IMPL_H_
