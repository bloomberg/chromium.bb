// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTER_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTER_H_

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"

namespace mojo {
// TODO(https://crbug.com/789854) Remove these converters once
// ServiceWorkerFetchRequest is removed.
template <>
struct TypeConverter<blink::mojom::FetchAPIRequestPtr,
                     content::ServiceWorkerFetchRequest> {
  CONTENT_EXPORT static blink::mojom::FetchAPIRequestPtr Convert(
      const content::ServiceWorkerFetchRequest& request);
};

template <>
struct TypeConverter<content::ServiceWorkerFetchRequest,
                     blink::mojom::FetchAPIRequest> {
  CONTENT_EXPORT static content::ServiceWorkerFetchRequest Convert(
      const blink::mojom::FetchAPIRequest& request);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTER_H_