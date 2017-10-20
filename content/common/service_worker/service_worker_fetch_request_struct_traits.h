// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_FETCH_REQUEST_STRUCT_TRAITS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_FETCH_REQUEST_STRUCT_TRAITS_H_

#include "base/numerics/safe_conversions.h"
#include "content/public/common/referrer.h"
#include "storage/common/blob_storage/blob_handle.h"
#include "third_party/WebKit/public/platform/modules/fetch/fetch_api_request.mojom.h"

namespace mojo {

template <>
struct EnumTraits<blink::mojom::FetchCredentialsMode,
                  content::FetchCredentialsMode> {
  static blink::mojom::FetchCredentialsMode ToMojom(
      content::FetchCredentialsMode input);

  static bool FromMojom(blink::mojom::FetchCredentialsMode input,
                        content::FetchCredentialsMode* out);
};

template <>
struct EnumTraits<blink::mojom::FetchRedirectMode, content::FetchRedirectMode> {
  static blink::mojom::FetchRedirectMode ToMojom(
      content::FetchRedirectMode input);

  static bool FromMojom(blink::mojom::FetchRedirectMode input,
                        content::FetchRedirectMode* out);
};

template <>
struct EnumTraits<blink::mojom::FetchRequestMode, content::FetchRequestMode> {
  static blink::mojom::FetchRequestMode ToMojom(
      content::FetchRequestMode input);

  static bool FromMojom(blink::mojom::FetchRequestMode input,
                        content::FetchRequestMode* out);
};

template <>
struct EnumTraits<blink::mojom::RequestContextFrameType,
                  content::RequestContextFrameType> {
  static blink::mojom::RequestContextFrameType ToMojom(
      content::RequestContextFrameType input);

  static bool FromMojom(blink::mojom::RequestContextFrameType input,
                        content::RequestContextFrameType* out);
};

template <>
struct EnumTraits<blink::mojom::RequestContextType,
                  content::RequestContextType> {
  static blink::mojom::RequestContextType ToMojom(
      content::RequestContextType input);

  static bool FromMojom(blink::mojom::RequestContextType input,
                        content::RequestContextType* out);
};

template <>
struct EnumTraits<blink::mojom::ServiceWorkerFetchType,
                  content::ServiceWorkerFetchType> {
  static blink::mojom::ServiceWorkerFetchType ToMojom(
      content::ServiceWorkerFetchType input);

  static bool FromMojom(blink::mojom::ServiceWorkerFetchType input,
                        content::ServiceWorkerFetchType* out);
};

template <>
struct StructTraits<blink::mojom::FetchAPIRequestDataView,
                    content::ServiceWorkerFetchRequest> {
  static content::FetchRequestMode mode(
      const content::ServiceWorkerFetchRequest& request) {
    return request.mode;
  }

  static bool is_main_resource_load(
      const content::ServiceWorkerFetchRequest& request) {
    return request.is_main_resource_load;
  }

  static content::RequestContextType request_context_type(
      const content::ServiceWorkerFetchRequest& request) {
    return request.request_context_type;
  }

  static content::RequestContextFrameType frame_type(
      const content::ServiceWorkerFetchRequest& request) {
    return request.frame_type;
  }

  static const GURL& url(const content::ServiceWorkerFetchRequest& request) {
    return request.url;
  }

  static const std::string& method(
      const content::ServiceWorkerFetchRequest& request) {
    return request.method;
  }

  static std::map<std::string, std::string> headers(
      const content::ServiceWorkerFetchRequest& request);

  static const std::string& blob_uuid(
      const content::ServiceWorkerFetchRequest& request) {
    return request.blob_uuid;
  }

  static uint64_t blob_size(const content::ServiceWorkerFetchRequest& request) {
    return request.blob_size;
  }

  static blink::mojom::BlobPtr blob(
      const content::ServiceWorkerFetchRequest& request) {
    if (!request.blob)
      return nullptr;
    return request.blob->Clone();
  }

  static const content::Referrer& referrer(
      const content::ServiceWorkerFetchRequest& request) {
    return request.referrer;
  }

  static content::FetchCredentialsMode credentials_mode(
      const content::ServiceWorkerFetchRequest& request) {
    return request.credentials_mode;
  }

  static content::FetchRedirectMode redirect_mode(
      const content::ServiceWorkerFetchRequest& request) {
    return request.redirect_mode;
  }

  static const std::string& integrity(
      const content::ServiceWorkerFetchRequest& request) {
    return request.integrity;
  }

  static const std::string& client_id(
      const content::ServiceWorkerFetchRequest& request) {
    return request.client_id;
  }

  static bool is_reload(const content::ServiceWorkerFetchRequest& request) {
    return request.is_reload;
  }

  static content::ServiceWorkerFetchType fetch_type(
      const content::ServiceWorkerFetchRequest& request) {
    return request.fetch_type;
  }

  static bool Read(blink::mojom::FetchAPIRequestDataView data,
                   content::ServiceWorkerFetchRequest* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_FETCH_REQUEST_STRUCT_TRAITS_H_
