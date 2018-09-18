// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_loader_helpers.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/stringprintf.h"
#include "content/public/common/content_features.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/http/http_util.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/resource_response.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
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

// Sets |has_range_out| to true if |headers| specify a single range request, and
// |offset_out| and |size_out| to the range. Returns true on valid input
// (regardless of |has_range_out|), and false if there is more than one range or
// if the bounds overflow.
bool ExtractSinglePartHttpRange(const net::HttpRequestHeaders& headers,
                                bool* has_range_out,
                                uint64_t* offset_out,
                                uint64_t* length_out) {
  std::string range_header;
  *has_range_out = false;
  if (!headers.GetHeader(net::HttpRequestHeaders::kRange, &range_header))
    return true;

  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRangeHeader(range_header, &ranges))
    return true;

  // Multi-part (or invalid) ranges are not supported.
  if (ranges.size() != 1)
    return false;

  // Safely parse the single range to our more-sane output format.
  *has_range_out = true;
  const net::HttpByteRange& byte_range = ranges[0];
  if (byte_range.first_byte_position() < 0)
    return false;
  // Allow the range [0, -1] to be valid and specify the entire range.
  if (byte_range.first_byte_position() == 0 &&
      byte_range.last_byte_position() == -1) {
    *has_range_out = false;
    return true;
  }
  if (byte_range.last_byte_position() < 0)
    return false;

  uint64_t first_byte_position =
      static_cast<uint64_t>(byte_range.first_byte_position());
  uint64_t last_byte_position =
      static_cast<uint64_t>(byte_range.last_byte_position());

  base::CheckedNumeric<uint64_t> length = last_byte_position;
  length -= first_byte_position;
  length += 1;

  if (!length.IsValid())
    return false;

  *offset_out = static_cast<uint64_t>(byte_range.first_byte_position());
  *length_out = length.ValueOrDie();
  return true;
}

}  // namespace

// static
void ServiceWorkerLoaderHelpers::SaveResponseHeaders(
    const int status_code,
    const std::string& status_text,
    const base::flat_map<std::string, std::string>& headers,
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

  // Populate |out_head|'s MIME type with the value from the HTTP response
  // headers.
  if (out_head->mime_type.empty()) {
    std::string mime_type;
    if (out_head->headers->GetMimeType(&mime_type))
      out_head->mime_type = mime_type;
  }

  // Populate |out_head|'s charset with the value from the HTTP response
  // headers.
  if (out_head->charset.empty()) {
    std::string charset;
    if (out_head->headers->GetCharset(&charset))
      out_head->charset = charset;
  }
}

// static
void ServiceWorkerLoaderHelpers::SaveResponseInfo(
    const blink::mojom::FetchAPIResponse& response,
    network::ResourceResponseHead* out_head) {
  out_head->was_fetched_via_service_worker = true;
  out_head->was_fallback_required_by_service_worker = false;
  out_head->url_list_via_service_worker = response.url_list;
  out_head->response_type = response.response_type;
  out_head->response_time = response.response_time;
  out_head->is_in_cache_storage = response.is_in_cache_storage;
  if (response.cache_storage_cache_name)
    out_head->cache_storage_cache_name = *(response.cache_storage_cache_name);
  else
    out_head->cache_storage_cache_name.clear();
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
      original_request.url.Resolve(new_location), false,
      token_binding_negotiated);
}

int ServiceWorkerLoaderHelpers::ReadBlobResponseBody(
    blink::mojom::BlobPtr* blob,
    uint64_t blob_size,
    const net::HttpRequestHeaders& headers,
    base::OnceCallback<void(int)> on_blob_read_complete,
    mojo::ScopedDataPipeConsumerHandle* handle_out) {
  bool byte_range_set = false;
  uint64_t offset = 0;
  uint64_t length = 0;
  // We don't support multiple range requests in one single URL request,
  // because we need to do multipart encoding here.
  // TODO(falken): Support multipart byte range requests.
  if (!ExtractSinglePartHttpRange(headers, &byte_range_set, &offset, &length)) {
    return net::ERR_REQUEST_RANGE_NOT_SATISFIABLE;
  }

  mojo::DataPipe data_pipe(blink::BlobUtils::GetDataPipeCapacity(blob_size));
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
