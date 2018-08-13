// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/background_fetch/background_fetch_types.h"

namespace {

blink::mojom::SerializedBlobPtr MakeCloneSerializedBlob(
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

BackgroundFetchOptions::BackgroundFetchOptions() = default;

BackgroundFetchOptions::BackgroundFetchOptions(
    const BackgroundFetchOptions& other) = default;

BackgroundFetchOptions::~BackgroundFetchOptions() = default;

BackgroundFetchRegistration::BackgroundFetchRegistration() = default;

BackgroundFetchRegistration::BackgroundFetchRegistration(
    const BackgroundFetchRegistration& other) = default;

BackgroundFetchRegistration::~BackgroundFetchRegistration() = default;

// static
blink::mojom::FetchAPIResponsePtr
BackgroundFetchSettledFetch::MakeCloneResponse(
    const blink::mojom::FetchAPIResponsePtr& response) {
  if (!response)
    return nullptr;
  return blink::mojom::FetchAPIResponse::New(
      response->url_list, response->status_code, response->status_text,
      response->response_type, response->headers,
      MakeCloneSerializedBlob(response->blob), response->error,
      response->response_time, response->cache_storage_cache_name,
      response->cors_exposed_header_names, response->is_in_cache_storage,
      MakeCloneSerializedBlob(response->side_data_blob));
}
BackgroundFetchSettledFetch::BackgroundFetchSettledFetch() = default;

BackgroundFetchSettledFetch::BackgroundFetchSettledFetch(
    const BackgroundFetchSettledFetch& other) {
  *this = other;
}

BackgroundFetchSettledFetch& BackgroundFetchSettledFetch::operator=(
    const BackgroundFetchSettledFetch& other) {
  request = other.request;
  response = MakeCloneResponse(other.response);
  return *this;
}

BackgroundFetchSettledFetch::~BackgroundFetchSettledFetch() = default;

}  // namespace content
