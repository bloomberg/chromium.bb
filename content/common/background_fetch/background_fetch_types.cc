// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_types.h"
#include "third_party/blink/public/platform/modules/background_fetch/background_fetch.mojom.h"

namespace {

blink::mojom::SerializedBlobPtr CloneSerializedBlob(
    const blink::mojom::SerializedBlobPtr& blob) {
  if (!blob)
    return nullptr;
  blink::mojom::BlobPtr blob_ptr(std::move(blob->blob));
  blob_ptr->Clone(mojo::MakeRequest(&blob->blob));
  return blink::mojom::SerializedBlob::New(
      blob->uuid, blob->content_type, blob->size, blob_ptr.PassInterface());
}

}  // namespace

namespace content {

// static
blink::mojom::FetchAPIResponsePtr BackgroundFetchSettledFetch::CloneResponse(
    const blink::mojom::FetchAPIResponsePtr& response) {
  // TODO(https://crbug.com/876546): Replace this method with response.Clone()
  // if the associated bug is fixed.
  if (!response)
    return nullptr;
  return blink::mojom::FetchAPIResponse::New(
      response->url_list, response->status_code, response->status_text,
      response->response_type, response->response_source, response->headers,
      CloneSerializedBlob(response->blob), response->error,
      response->response_time, response->cache_storage_cache_name,
      response->cors_exposed_header_names, response->is_in_cache_storage,
      CloneSerializedBlob(response->side_data_blob));
}

// static
blink::mojom::FetchAPIRequestPtr BackgroundFetchSettledFetch::CloneRequest(
    const blink::mojom::FetchAPIRequestPtr& request) {
  if (!request)
    return nullptr;
  return blink::mojom::FetchAPIRequest::New(
      request->mode, request->is_main_resource_load,
      request->request_context_type, request->frame_type, request->url,
      request->method, request->headers, CloneSerializedBlob(request->blob),
      request->referrer.Clone(), request->credentials_mode, request->cache_mode,
      request->redirect_mode, request->integrity, request->keepalive,
      request->client_id, request->is_reload, request->is_history_navigation);
}

BackgroundFetchSettledFetch::BackgroundFetchSettledFetch() = default;

BackgroundFetchSettledFetch::BackgroundFetchSettledFetch(
    const BackgroundFetchSettledFetch& other) {
  *this = other;
}

BackgroundFetchSettledFetch& BackgroundFetchSettledFetch::operator=(
    const BackgroundFetchSettledFetch& other) {
  request = CloneRequest(other.request);
  response = CloneResponse(other.response);
  return *this;
}

BackgroundFetchSettledFetch::~BackgroundFetchSettledFetch() = default;

}  // namespace content
