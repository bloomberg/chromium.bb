// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_loader_helpers.h"

#include <memory>
#include <utility>

#include "base/strings/stringprintf.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/loader_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_response.h"
#include "ui/base/page_transition_types.h"

namespace content {
namespace {

// Calls |callback| when Blob reading is complete.
class BlobCompleteCaller : public blink::mojom::BlobReaderClient {
 public:
  using BlobCompleteCallback = base::OnceCallback<void(int net_error)>;

  explicit BlobCompleteCaller(BlobCompleteCallback callback)
      : callback_(std::move(callback)) {}
  ~BlobCompleteCaller() override = default;

  void OnCalculatedSize(uint64_t total_size,
                        uint64_t expected_content_size) override {}
  void OnComplete(int32_t status, uint64_t data_length) override {
    std::move(callback_).Run(base::checked_cast<int>(status));
  }

 private:
  BlobCompleteCallback callback_;
};

}  // namespace

// static
std::unique_ptr<ServiceWorkerFetchRequest>
ServiceWorkerLoaderHelpers::CreateFetchRequest(
    const network::ResourceRequest& request) {
  auto new_request = std::make_unique<ServiceWorkerFetchRequest>();
  new_request->mode = request.fetch_request_mode;
  new_request->is_main_resource_load = ServiceWorkerUtils::IsMainResourceType(
      static_cast<ResourceType>(request.resource_type));
  new_request->request_context_type =
      static_cast<RequestContextType>(request.fetch_request_context_type);
  new_request->frame_type = request.fetch_frame_type;
  new_request->url = request.url;
  new_request->method = request.method;
  // |blob_uuid| and |blob_size| aren't used in MojoBlobs, so just clear them.
  // The caller is responsible for setting the MojoBlob field |blob| if needed.
  new_request->blob_uuid.clear();
  new_request->blob_size = 0;
  new_request->credentials_mode = request.fetch_credentials_mode;
  new_request->cache_mode =
      ServiceWorkerFetchRequest::GetCacheModeFromLoadFlags(request.load_flags);
  new_request->redirect_mode = request.fetch_redirect_mode;
  new_request->keepalive = request.keepalive;
  new_request->is_reload = ui::PageTransitionCoreTypeIs(
      static_cast<ui::PageTransition>(request.transition_type),
      ui::PAGE_TRANSITION_RELOAD);
  new_request->referrer = Referrer(
      GURL(request.referrer), Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
                                  request.referrer_policy));
  new_request->fetch_type = ServiceWorkerFetchType::FETCH;
  return new_request;
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseHeaders(
    const int status_code,
    const std::string& status_text,
    const ServiceWorkerHeaderMap& headers,
    network::ResourceResponseHead* out_head) {
  // Build a string instead of using HttpResponseHeaders::AddHeader on
  // each header, since AddHeader has O(n^2) performance.
  std::string buf(base::StringPrintf("HTTP/1.1 %d %s\r\n", status_code,
                                     status_text.c_str()));
  for (const auto& item : headers) {
    buf.append(item.first);
    buf.append(": ");
    buf.append(item.second);
    buf.append("\r\n");
  }
  buf.append("\r\n");

  out_head->headers = new net::HttpResponseHeaders(
      net::HttpUtil::AssembleRawHeaders(buf.c_str(), buf.size()));
  if (out_head->mime_type.empty()) {
    std::string mime_type;
    out_head->headers->GetMimeType(&mime_type);
    if (mime_type.empty())
      mime_type = "text/plain";
    out_head->mime_type = mime_type;
  }
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseInfo(
    const ServiceWorkerResponse& response,
    network::ResourceResponseHead* out_head) {
  out_head->was_fetched_via_service_worker = true;
  out_head->was_fallback_required_by_service_worker = false;
  out_head->url_list_via_service_worker = response.url_list;
  out_head->response_type_via_service_worker = response.response_type;
  out_head->is_in_cache_storage = response.is_in_cache_storage;
  out_head->cache_storage_cache_name = response.cache_storage_cache_name;
  out_head->cors_exposed_header_names = response.cors_exposed_header_names;
  out_head->did_service_worker_navigation_preload = false;
}

// static
base::Optional<net::RedirectInfo>
ServiceWorkerLoaderHelpers::ComputeRedirectInfo(
    const network::ResourceRequest& original_request,
    const network::ResourceResponseHead& response_head,
    bool token_binding_negotiated) {
  std::string new_location;
  if (!response_head.headers->IsRedirect(&new_location))
    return base::nullopt;

  // If the request is a MAIN_FRAME request, the first-party URL gets
  // updated on redirects.
  const net::URLRequest::FirstPartyURLPolicy first_party_url_policy =
      original_request.resource_type == RESOURCE_TYPE_MAIN_FRAME
          ? net::URLRequest::UPDATE_FIRST_PARTY_URL_ON_REDIRECT
          : net::URLRequest::NEVER_CHANGE_FIRST_PARTY_URL;
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies, first_party_url_policy,
      original_request.referrer_policy,
      network::ComputeReferrer(original_request.referrer),
      response_head.headers.get(), response_head.headers->response_code(),
      original_request.url.Resolve(new_location), token_binding_negotiated);
}

int ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
    blink::mojom::BlobPtr* blob,
    const net::HttpRequestHeaders& headers,
    base::OnceCallback<void(int)> on_blob_read_complete,
    mojo::ScopedDataPipeConsumerHandle* handle_out) {
  bool byte_range_set = false;
  uint64_t offset = 0;
  uint64_t length = 0;
  // We don't support multiple range requests in one single URL request,
  // because we need to do multipart encoding here.
  // TODO(falken): Support multipart byte range requests.
  if (!ServiceWorkerUtils::ExtractSinglePartHttpRange(headers, &byte_range_set,
                                                      &offset, &length)) {
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
  }

  mojo::DataPipe data_pipe;
  blink::mojom::BlobReaderClientPtr blob_reader_client;
  mojo::MakeStrongBinding(
      std::make_unique<BlobCompleteCaller>(std::move(on_blob_read_complete)),
      mojo::MakeRequest(&blob_reader_client));

  if (byte_range_set) {
    (*blob)->ReadRange(offset, length, std::move(data_pipe.producer_handle),
                       std::move(blob_reader_client));
  } else {
    (*blob)->ReadAll(std::move(data_pipe.producer_handle),
                     std::move(blob_reader_client));
  }
  *handle_out = std::move(data_pipe.consumer_handle);
  return net::OK;
}

}  // namespace content
