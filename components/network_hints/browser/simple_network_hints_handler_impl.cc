// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_hints/browser/simple_network_hints_handler_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "ipc/ipc_message_macros.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/log/net_log_with_source.h"
#include "services/network/public/cpp/resolve_host_client_base.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace network_hints {
namespace {

const int kDefaultPort = 80;

// This class contains a std::unique_ptr of itself, it is passed in through
// Start() method, and will be freed by the OnComplete() method when resolving
// has completed or mojo connection error has happened.
class DnsLookupRequest : public network::ResolveHostClientBase {
 public:
  DnsLookupRequest(int render_process_id,
                   int render_frame_id,
                   const std::string& hostname)
      : render_process_id_(render_process_id),
        render_frame_id_(render_frame_id),
        hostname_(hostname) {}

  // Return underlying network resolver status.
  // net::OK ==> Host was found synchronously.
  // net:ERR_IO_PENDING ==> Network will callback later with result.
  // anything else ==> Host was not found synchronously.
  void Start(std::unique_ptr<DnsLookupRequest> request) {
    request_ = std::move(request);

    content::RenderFrameHost* render_frame_host =
        content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
    if (!render_frame_host) {
      OnComplete(net::ERR_NAME_NOT_RESOLVED,
                 net::ResolveErrorInfo(net::ERR_FAILED), base::nullopt);
      return;
    }

    DCHECK(!receiver_.is_bound());
    net::HostPortPair host_port_pair(hostname_, kDefaultPort);
    network::mojom::ResolveHostParametersPtr resolve_host_parameters =
        network::mojom::ResolveHostParameters::New();
    // Lets the host resolver know it can be de-prioritized.
    resolve_host_parameters->initial_priority = net::RequestPriority::IDLE;
    // Make a note that this is a speculative resolve request. This allows
    // separating it from real navigations in the observer's callback.
    resolve_host_parameters->is_speculative = true;
    // TODO(https://crbug.com/997049): Pass in a non-empty NetworkIsolationKey.
    render_frame_host->GetProcess()
        ->GetStoragePartition()
        ->GetNetworkContext()
        ->ResolveHost(host_port_pair,
                      render_frame_host->GetNetworkIsolationKey(),
                      std::move(resolve_host_parameters),
                      receiver_.BindNewPipeAndPassRemote());
    receiver_.set_disconnect_handler(
        base::BindOnce(&DnsLookupRequest::OnComplete, base::Unretained(this),
                       net::ERR_NAME_NOT_RESOLVED,
                       net::ResolveErrorInfo(net::ERR_FAILED), base::nullopt));
  }

 private:
  // network::mojom::ResolveHostClient:
  void OnComplete(
      int result,
      const net::ResolveErrorInfo& resolve_error_info,
      const base::Optional<net::AddressList>& resolved_addresses) override {
    VLOG(2) << __FUNCTION__ << ": " << hostname_
            << ", result=" << resolve_error_info.error;
    request_.reset();
  }

  mojo::Receiver<network::mojom::ResolveHostClient> receiver_{this};
  const int render_process_id_;
  const int render_frame_id_;
  const std::string hostname_;
  std::unique_ptr<DnsLookupRequest> request_;

  DISALLOW_COPY_AND_ASSIGN(DnsLookupRequest);
};

}  // namespace

SimpleNetworkHintsHandlerImpl::SimpleNetworkHintsHandlerImpl(
    int render_process_id,
    int render_frame_id)
    : render_process_id_(render_process_id),
      render_frame_id_(render_frame_id) {}

SimpleNetworkHintsHandlerImpl::~SimpleNetworkHintsHandlerImpl() = default;

// static
void SimpleNetworkHintsHandlerImpl::Create(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mojom::NetworkHintsHandler> receiver) {
  int render_process_id = frame_host->GetProcess()->GetID();
  int render_frame_id = frame_host->GetRoutingID();
  mojo::MakeSelfOwnedReceiver(
      base::WrapUnique(new SimpleNetworkHintsHandlerImpl(render_process_id,
                                                         render_frame_id)),
      std::move(receiver));
}

void SimpleNetworkHintsHandlerImpl::PrefetchDNS(
    const std::vector<std::string>& names) {
  for (const std::string& hostname : names) {
    std::unique_ptr<DnsLookupRequest> request =
        std::make_unique<DnsLookupRequest>(render_process_id_, render_frame_id_,
                                           hostname);
    DnsLookupRequest* request_ptr = request.get();
    request_ptr->Start(std::move(request));
  }
}

void SimpleNetworkHintsHandlerImpl::Preconnect(const GURL& url,
                                               bool allow_credentials) {
  // Not implemented.
}

}  // namespace network_hints
