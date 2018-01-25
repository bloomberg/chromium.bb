// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/web_package_request_handler.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "content/browser/loader/web_package_loader.h"
#include "content/common/throttling_url_loader.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/interfaces/url_loader.mojom.h"

namespace content {

// static
bool WebPackageRequestHandler::IsSupportedMimeType(
    const std::string& mime_type) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));
  return mime_type == "application/http-exchange+cbor";
}

WebPackageRequestHandler::WebPackageRequestHandler() : weak_factory_(this) {
  DCHECK(base::FeatureList::IsEnabled(features::kSignedHTTPExchange));
}

WebPackageRequestHandler::~WebPackageRequestHandler() = default;

void WebPackageRequestHandler::MaybeCreateLoader(
    const network::ResourceRequest& resource_request,
    ResourceContext* resource_context,
    LoaderCallback callback) {
  // TODO(https://crbug.com/80374): Ask WebPackageFetchManager to get the
  // ongoing matching SignedExchangeHandler which was created by a
  // WebPackagePrefetcher.

  if (!web_package_loader_) {
    std::move(callback).Run(StartLoaderCallback());
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

  // TODO(https://crbug.com/80374): Consider creating a new ThrottlingURLLoader
  // or reusing the existing ThrottlingURLLoader by reattaching URLLoaderClient,
  // to support SafeBrowsing checking of the content of the WebPackage.
  web_package_loader_ = base::MakeUnique<WebPackageLoader>(
      response, std::move(client), url_loader->Unbind());
  return true;
}

void WebPackageRequestHandler::StartResponse(
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  web_package_loader_->ConnectToClient(std::move(client));
  mojo::MakeStrongBinding(std::move(web_package_loader_), std::move(request));
}

}  // namespace content
