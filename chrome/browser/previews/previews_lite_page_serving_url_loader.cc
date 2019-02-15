// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_serving_url_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/previews_state.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace previews {

namespace {
// Used for mojo pipe size. Same constant as navigation code.
constexpr size_t kServingDefaultAllocationSize = 512 * 1024;

const net::NetworkTrafficAnnotationTag kPreviewsTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("https_server_previews_navigation", R"(
      semantics {
        sender: "HTTPS server previews navigation URL Loader"
        description:
          "This request is issued by a main frame navigation to fetch a server "
          "generated lite page version of page."
        trigger:
          "Navigating Chrome (by clicking on a link, bookmark, history item, "
          "using session restore, etc) on a slow page."
        data:
          "Arbitrary site-controlled data can be included in the URL, HTTP "
          "headers, and request body."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "Disable Data Saver on Android (Settings > Data Saver)."
        chrome_policy {
          URLBlacklist {
            URLBlacklist: { entries: '*' }
          }
        }
        chrome_policy {
          URLWhitelist {
            URLWhitelist { }
          }
        }
      }
      )");

}  // namespace

PreviewsLitePageServingURLLoader::PreviewsLitePageServingURLLoader(
    ResultCallback result_callback)
    : url_loader_binding_(this),
      result_callback_(std::move(result_callback)),
      binding_(this),
      weak_ptr_factory_(this) {}

void PreviewsLitePageServingURLLoader::StartNetworkRequest(
    const network::ResourceRequest& request,
    const scoped_refptr<network::SharedURLLoaderFactory>&
        network_loader_factory,
    int frame_tree_node_id) {
  network::mojom::URLLoaderClientPtr client;

  url_loader_binding_.Bind(mojo::MakeRequest(&client),
                           base::ThreadTaskRunnerHandle::Get());
  url_loader_binding_.set_connection_error_handler(
      base::BindOnce(&PreviewsLitePageServingURLLoader::OnConnectionError,
                     base::Unretained(this)));

  // Create a network service URL loader with passed in params.
  network_loader_factory->CreateLoaderAndStart(
      mojo::MakeRequest(&network_url_loader_), frame_tree_node_id, 0,
      network::mojom::kURLLoadOptionNone, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(kPreviewsTrafficAnnotation));
}

PreviewsLitePageServingURLLoader::~PreviewsLitePageServingURLLoader() = default;

void PreviewsLitePageServingURLLoader::Fallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(result_callback_).Run(ServingLoaderResult::kFallback);
}

RequestHandler PreviewsLitePageServingURLLoader::ServingResponseHandler() {
  DCHECK(result_callback_.is_null());
  return base::BindOnce(
      &PreviewsLitePageServingURLLoader::SetUpForwardingClient,
      weak_ptr_factory_.GetWeakPtr());
}

void PreviewsLitePageServingURLLoader::SetUpForwardingClient(
    const network::ResourceRequest& /* resource_request */,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr forwarding_client) {
  // Bind to the content/ navigation code.
  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      base::BindOnce(&PreviewsLitePageServingURLLoader::OnConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));
  forwarding_client_ = std::move(forwarding_client);

  // If there was an URLLoader error between handing off this handler and
  // running it, don't handle the request.
  if (!network_url_loader_) {
    binding_.Close();
    forwarding_client_.reset();
    delete this;
    return;
  }

  mojo::DataPipe pipe(kServingDefaultAllocationSize);
  if (!pipe.consumer_handle.is_valid()) {
    network_url_loader_.reset();
    url_loader_binding_.Close();
    delete this;
    return;
  }

  forwarding_client_->OnReceiveResponse(resource_response_->head);

  // Resume previously paused network service URLLoader.
  url_loader_binding_.ResumeIncomingMethodCallProcessing();
}

void PreviewsLitePageServingURLLoader::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  DCHECK(!forwarding_client_);

  // TODO: evaluate all the responses we allow, don't hard code 200.
  if (head.headers->response_code() != net::HTTP_OK) {
    Fallback();
    return;
  }

  // Store head and pause new messages until the forwarding client is set up.
  // Make a deep copy of ResourceResponseHead before passing it cross-thread.
  resource_response_ = base::MakeRefCounted<network::ResourceResponse>();
  resource_response_->head = head;

  url_loader_binding_.PauseIncomingMethodCallProcessing();
  std::move(result_callback_).Run(ServingLoaderResult::kSuccess);
}

void PreviewsLitePageServingURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  DCHECK(!forwarding_client_);
  // We might receive a redirect from the lite page server, and we should treat
  // that appropriately. For now fallback.
  // TODO(ryansturm): Handle redirects. https://crbug.com/921744
  Fallback();
}

void PreviewsLitePageServingURLLoader::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  // We only handle GETs.
  NOTREACHED();
}

void PreviewsLitePageServingURLLoader::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  // Do nothing. This is not supported for navigation loader.
}

void PreviewsLitePageServingURLLoader::OnTransferSizeUpdated(
    int32_t transfer_size_diff) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void PreviewsLitePageServingURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(forwarding_client_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void PreviewsLitePageServingURLLoader::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (forwarding_client_) {
    forwarding_client_->OnComplete(status);
    return;
  }

  // If OnComplete is called before, OnReceiveResponse, this is indicative of a
  // failure of some sort.
  // TODO(ryansturm): Handle specific errors with a bypass pattern.
  // https://crbug.com/921744
  Fallback();
}

void PreviewsLitePageServingURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const base::Optional<GURL>& new_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This should never be called for a non-network service URLLoader.
  NOTREACHED();
}

void PreviewsLitePageServingURLLoader::ProceedWithResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  network_url_loader_->ProceedWithResponse();
}

void PreviewsLitePageServingURLLoader::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  network_url_loader_->SetPriority(priority, intra_priority_value);
}

void PreviewsLitePageServingURLLoader::PauseReadingBodyFromNet() {
  // Pass through.
  network_url_loader_->PauseReadingBodyFromNet();
}

void PreviewsLitePageServingURLLoader::ResumeReadingBodyFromNet() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Pass through.
  network_url_loader_->ResumeReadingBodyFromNet();
}

void PreviewsLitePageServingURLLoader::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // When we are not yet bound to the navigation code, fallback, which will
  // destroy this object.
  if (!result_callback_.is_null()) {
    Fallback();
    return;
  }

  network_url_loader_.reset();
  url_loader_binding_.Close();

  if (binding_.is_bound()) {
    binding_.Close();
    forwarding_client_.reset();
    delete this;
  }
}

}  // namespace previews
