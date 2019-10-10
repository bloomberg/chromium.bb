// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_extension_url_loader_factory.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chromecast/common/cast_redirect_manifest_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace chromecast {
namespace shell {

namespace {

class CastExtensionURLLoader : public network::mojom::URLLoader,
                               public network::mojom::URLLoaderClient {
 public:
  static void CreateAndStart(
      network::mojom::URLLoaderRequest loader_request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      scoped_refptr<network::SharedURLLoaderFactory> network_factory) {
    // Owns itself. Will live as long as its URLLoader and URLLoaderClientPtr
    // bindings are alive - essentially until either the client gives up or all
    // data has been sent to it.
    auto* cast_extension_url_loader = new CastExtensionURLLoader(
        std::move(loader_request), std::move(client));
    cast_extension_url_loader->Start(routing_id, request_id, options,
                                     std::move(request), traffic_annotation,
                                     network_factory);
  }

 private:
  CastExtensionURLLoader(network::mojom::URLLoaderRequest loader_request,
                         network::mojom::URLLoaderClientPtr client)
      : original_loader_binding_(this, std::move(loader_request)),
        original_client_(std::move(client)),
        network_client_binding_(this) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    // If there is a client error, clean up the request.
    original_client_.set_connection_error_handler(base::BindOnce(
        &CastExtensionURLLoader::OnClientError, base::Unretained(this)));
  }

  ~CastExtensionURLLoader() override = default;

  void Start(int32_t routing_id,
             int32_t request_id,
             uint32_t options,
             const network::ResourceRequest& request,
             const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
             scoped_refptr<network::SharedURLLoaderFactory> network_factory) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    network::mojom::URLLoaderClientPtr network_client;
    network_client_binding_.Bind(mojo::MakeRequest(&network_client));

    network_factory->CreateLoaderAndStart(
        mojo::MakeRequest(&network_loader_), routing_id, request_id, options,
        request, std::move(network_client), traffic_annotation);

    network_client_binding_.set_connection_error_handler(base::BindOnce(
        &CastExtensionURLLoader::OnNetworkError, base::Unretained(this)));
  }

  void OnClientError() { delete this; }

  void OnNetworkError() {
    if (original_client_)
      original_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    delete this;
  }

  // network::mojom::URLLoader implementation:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {
    NOTREACHED()
        << "The original client shouldn't have been notified of any redirects";
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    network_loader_->SetPriority(priority, intra_priority_value);
  }

  void PauseReadingBodyFromNet() override {
    network_loader_->PauseReadingBodyFromNet();
  }

  void ResumeReadingBodyFromNet() override {
    network_loader_->ResumeReadingBodyFromNet();
  }

  // network::mojom::URLLoaderClient:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr head) override {
    original_client_->OnReceiveResponse(std::move(head));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    // Don't tell the original client since it thinks this is a local load and
    // just follow the redirect.
    network_loader_->FollowRedirect(std::vector<std::string>(),
                                    net::HttpRequestHeaders(), base::nullopt);
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    original_client_->OnUploadProgress(current_position, total_size,
                                       std::move(callback));
  }

  void OnReceiveCachedMetadata(mojo_base::BigBuffer data) override {
    original_client_->OnReceiveCachedMetadata(std::move(data));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    original_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    original_client_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    original_client_->OnComplete(status);
    delete this;
  }

  // This is the URLLoader that was passed in to
  // CastExtensionURLLoaderFactory.
  mojo::Binding<network::mojom::URLLoader> original_loader_binding_;

  // This is the URLLoaderClient that was passed in to
  // CastExtensionURLLoaderFactory. We'll send the data to it but not
  // information about redirects etc... as it should think the extension
  // resources are loaded locally.
  network::mojom::URLLoaderClientPtr original_client_;

  // This is the URLLoaderClient passed to the network URLLoaderFactory.
  mojo::Binding<network::mojom::URLLoaderClient> network_client_binding_;

  // This is the URLLoader from the network URLLoaderFactory.
  network::mojom::URLLoaderPtr network_loader_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionURLLoader);
};

}  // namespace

CastExtensionURLLoaderFactory::CastExtensionURLLoaderFactory(
    content::BrowserContext* browser_context,
    std::unique_ptr<network::mojom::URLLoaderFactory> extension_factory)
    : extension_registry_(extensions::ExtensionRegistry::Get(browser_context)),
      extension_factory_(std::move(extension_factory)),
      network_factory_(
          content::BrowserContext::GetDefaultStoragePartition(browser_context)
              ->GetURLLoaderFactoryForBrowserProcess()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CastExtensionURLLoaderFactory::~CastExtensionURLLoaderFactory() = default;

void CastExtensionURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  const GURL& url = request.url;
  const extensions::Extension* extension =
      extension_registry_->enabled_extensions().GetExtensionOrAppByURL(url);
  if (!extension) {
    LOG(ERROR) << "Can't find extension with id: " << url.host();
    return;
  }

  std::string cast_url;
  // See if we are being redirected to an extension-specific URL.
  if (!CastRedirectHandler::ParseUrl(&cast_url, extension, url)) {
    // Defer to the default handler to load from disk.
    extension_factory_->CreateLoaderAndStart(
        std::move(loader_request), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  // The above only handles the scheme, host & path, any query or fragment needs
  // to be copied separately.
  if (url.has_query()) {
    cast_url.push_back('?');
    url.query_piece().AppendToString(&cast_url);
  }

  if (url.has_ref()) {
    cast_url.push_back('#');
    url.ref_piece().AppendToString(&cast_url);
  }

  network::ResourceRequest new_request(request);
  new_request.url = GURL(cast_url);
  new_request.site_for_cookies = new_request.url;

  // Force a redirect to the new URL but without changing where the webpage
  // thinks it is.
  CastExtensionURLLoader::CreateAndStart(
      std::move(loader_request), routing_id, request_id, options,
      std::move(new_request), std::move(client), traffic_annotation,
      network_factory_);
}

void CastExtensionURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver) {
  receivers_.Add(this, std::move(factory_receiver));
}

}  // namespace shell
}  // namespace chromecast
