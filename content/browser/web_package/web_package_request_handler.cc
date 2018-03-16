// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_package_request_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/browser/web_package/web_package_loader.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"

namespace content {

// static
bool WebPackageRequestHandler::IsSupportedMimeType(
    const std::string& mime_type) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));
  return mime_type == "application/signed-exchange";
}

WebPackageRequestHandler::WebPackageRequestHandler(
    url::Origin request_initiator,
    uint32_t url_loader_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter)
    : request_initiator_(std::move(request_initiator)),
      url_loader_options_(url_loader_options),
      url_loader_factory_(url_loader_factory),
      url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
      request_context_getter_(std::move(request_context_getter)),
      weak_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));
}

WebPackageRequestHandler::~WebPackageRequestHandler() = default;

void WebPackageRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& resource_request,
    ResourceContext* resource_context,
    LoaderCallback callback) {
  // TODO(https://crbug.com/803774): Ask WebPackageFetchManager to get the
  // ongoing matching SignedExchangeHandler which was created by a
  // WebPackagePrefetcher.

  if (!web_package_loader_) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(base::BindOnce(
      &WebPackageRequestHandler::StartResponse, weak_factory_.GetWeakPtr()));
}

bool WebPackageRequestHandler::MaybeCreateLoaderForResponse(
    const network::ResourceResponseHead& response,
    network::mojom::URLLoaderPtr* loader,
    network::mojom::URLLoaderClientRequest* client_request,
    ThrottlingURLLoader* url_loader) {
  std::string mime_type;
  if (response.was_fetched_via_service_worker || !response.headers ||
      !response.headers->GetMimeType(&mime_type) ||
      !IsSupportedMimeType(mime_type)) {
    return false;
  }

  network::mojom::URLLoaderClientPtr client;
  *client_request = mojo::MakeRequest(&client);

  // TODO(https://crbug.com/803774): Consider creating a new ThrottlingURLLoader
  // or reusing the existing ThrottlingURLLoader by reattaching URLLoaderClient,
  // to support SafeBrowsing checking of the content of the WebPackage.
  web_package_loader_ = std::make_unique<WebPackageLoader>(
      response, std::move(client), url_loader->Unbind(),
      std::move(request_initiator_), url_loader_options_,
      std::move(url_loader_factory_), std::move(url_loader_throttles_getter_),
      std::move(request_context_getter_));
  return true;
}

void WebPackageRequestHandler::StartResponse(
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  web_package_loader_->ConnectToClient(std::move(client));
  mojo::MakeStrongBinding(std::move(web_package_loader_), std::move(request));
}

}  // namespace content
