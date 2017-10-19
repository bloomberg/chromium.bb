// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_NETWORK_SERVICE_IMPL_H_
#define CONTENT_NETWORK_NETWORK_SERVICE_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/network_service.mojom.h"
#include "content/public/network/network_service.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/service.h"

namespace net {
class NetLog;
class LoggingNetworkChangeObserver;
class URLRequestContext;
class URLRequestContextBuilder;
}  // namespace net

namespace content {

class NetworkContext;

class CONTENT_EXPORT NetworkServiceImpl : public service_manager::Service,
                                          public NetworkService {
 public:
  // |net_log| is an optional shared NetLog, which will be used instead of the
  // service's own NetLog. It must outlive the NetworkService.
  //
  // TODO(https://crbug.com/767450): Once the NetworkService can always create
  // its own NetLog in production, remove the |net_log| argument.
  NetworkServiceImpl(std::unique_ptr<service_manager::BinderRegistry> registry,
                     net::NetLog* net_log = nullptr);

  ~NetworkServiceImpl() override;

  std::unique_ptr<mojom::NetworkContext> CreateNetworkContextWithBuilder(
      content::mojom::NetworkContextRequest request,
      content::mojom::NetworkContextParamsPtr params,
      std::unique_ptr<net::URLRequestContextBuilder> builder,
      net::URLRequestContext** url_request_context) override;

  static std::unique_ptr<NetworkServiceImpl> CreateForTesting();

  // These are called by NetworkContexts as they are being created and
  // destroyed.
  void RegisterNetworkContext(NetworkContext* network_context);
  void DeregisterNetworkContext(NetworkContext* network_context);

  // mojom::NetworkService implementation:
  void CreateNetworkContext(mojom::NetworkContextRequest request,
                            mojom::NetworkContextParamsPtr params) override;
  void DisableQuic() override;
  void SetRawHeadersAccess(uint32_t process_id, bool allow) override;

  bool quic_disabled() const { return quic_disabled_; }
  bool HasRawHeadersAccess(uint32_t process_id) const;

  net::NetLog* net_log() const;

 private:
  class MojoNetLog;

  friend class NetworkService;

  // service_manager::Service implementation.
  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override;

  void Create(mojom::NetworkServiceRequest request);

  std::unique_ptr<MojoNetLog> owned_net_log_;
  // TODO(https://crbug.com/767450): Remove this, once Chrome no longer creates
  // its own NetLog.
  net::NetLog* net_log_;

  // Observer that logs network changes to the NetLog. Must be below the NetLog
  // and the NetworkChangeNotifier (Once this class creates it), so it's
  // destroyed before them.
  std::unique_ptr<net::LoggingNetworkChangeObserver> network_change_observer_;

  std::unique_ptr<service_manager::BinderRegistry> registry_;

  mojo::Binding<mojom::NetworkService> binding_;

  // NetworkContexts register themselves with the NetworkService so that they
  // can be cleaned up when the NetworkService goes away. This is needed as
  // NetworkContexts share global state with the NetworkService, so must be
  // destroyed first.
  std::set<NetworkContext*> network_contexts_;
  std::set<uint32_t> processes_with_raw_headers_access_;

  bool quic_disabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceImpl);
};

}  // namespace content

#endif  // CONTENT_NETWORK_NETWORK_SERVICE_IMPL_H_
