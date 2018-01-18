// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/network_service_client.h"

#include "content/browser/ssl/ssl_error_handler.h"
#include "content/browser/ssl/ssl_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"

namespace content {
namespace {

class SSLDelegate : public SSLErrorHandler::Delegate {
 public:
  explicit SSLDelegate(
      network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback
          response)
      : response_(std::move(response)), weak_factory_(this) {}
  ~SSLDelegate() override {}
  void CancelSSLRequest(int error, const net::SSLInfo* ssl_info) override {
    std::move(response_).Run(error);
    delete this;
  }
  void ContinueSSLRequest() override {
    std::move(response_).Run(net::OK);
    delete this;
  }
  base::WeakPtr<SSLDelegate> GetWeakPtr() { return weak_factory_.GetWeakPtr(); }

 private:
  network::mojom::NetworkServiceClient::OnSSLCertificateErrorCallback response_;
  base::WeakPtrFactory<SSLDelegate> weak_factory_;
};

}  // namespace

NetworkServiceClient::NetworkServiceClient(
    network::mojom::NetworkServiceClientRequest network_service_client_request)
    : binding_(this, std::move(network_service_client_request)) {}

NetworkServiceClient::~NetworkServiceClient() = default;

void NetworkServiceClient::OnSSLCertificateError(
    int32_t resource_type,
    const GURL& url,
    uint32_t process_id,
    uint32_t routing_id,
    const net::SSLInfo& ssl_info,
    bool fatal,
    OnSSLCertificateErrorCallback response) {
  SSLDelegate* delegate = new SSLDelegate(std::move(response));  // deletes self
  base::Callback<WebContents*(void)> web_contents_getter =
      process_id ? base::Bind(WebContentsImpl::FromRenderFrameHostID,
                              process_id, routing_id)
                 : base::Bind(WebContents::FromFrameTreeNodeId, routing_id);
  SSLManager::OnSSLCertificateError(delegate->GetWeakPtr(),
                                    static_cast<ResourceType>(resource_type),
                                    url, web_contents_getter, ssl_info, fatal);
}

}  // namespace content
