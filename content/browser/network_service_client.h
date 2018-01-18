// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/interfaces/network_service.mojom.h"

namespace content {

class NetworkServiceClient : public network::mojom::NetworkServiceClient {
 public:
  explicit NetworkServiceClient(network::mojom::NetworkServiceClientRequest
                                    network_service_client_request);
  ~NetworkServiceClient() override;

  // network::mojom::NetworkServiceClient implementation:
  void OnSSLCertificateError(int32_t resource_type,
                             const GURL& url,
                             uint32_t process_id,
                             uint32_t routing_id,
                             const net::SSLInfo& ssl_info,
                             bool fatal,
                             OnSSLCertificateErrorCallback response) override;

 private:
  mojo::Binding<network::mojom::NetworkServiceClient> binding_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NETWORK_SERVICE_IMPL_H_
