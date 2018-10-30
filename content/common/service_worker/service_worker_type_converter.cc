// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_type_converter.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"

namespace mojo {

// static
blink::mojom::FetchAPIRequestPtr
TypeConverter<blink::mojom::FetchAPIRequestPtr,
              content::ServiceWorkerFetchRequest>::
    Convert(content::ServiceWorkerFetchRequest request) {
  auto request_ptr = blink::mojom::FetchAPIRequest::New();
  request_ptr->mode = request.mode;
  request_ptr->is_main_resource_load = request.is_main_resource_load;
  request_ptr->request_context_type = request.request_context_type;
  request_ptr->frame_type = request.frame_type;
  request_ptr->url = request.url;
  request_ptr->method = request.method;
  request_ptr->headers.insert(request.headers.begin(), request.headers.end());
  request_ptr->referrer = request.referrer;
  request_ptr->credentials_mode = request.credentials_mode;
  request_ptr->cache_mode = request.cache_mode;
  request_ptr->redirect_mode = request.redirect_mode;
  request_ptr->integrity = request.integrity;
  request_ptr->keepalive = request.keepalive;
  request_ptr->client_id = request.client_id;
  request_ptr->is_reload = request.is_reload;
  request_ptr->is_history_navigation = request.is_history_navigation;
  return request_ptr;
}

content::ServiceWorkerFetchRequest
TypeConverter<content::ServiceWorkerFetchRequest,
              blink::mojom::FetchAPIRequestPtr>::
    Convert(const blink::mojom::FetchAPIRequestPtr& request_ptr) {
  DCHECK(request_ptr);

  content::ServiceWorkerFetchRequest request;
  request.mode = request_ptr->mode;
  request.is_main_resource_load = request_ptr->is_main_resource_load;
  request.request_context_type = request_ptr->request_context_type;
  request.frame_type = request_ptr->frame_type;
  request.url = request_ptr->url;
  request.method = request_ptr->method;
  request.headers.insert(request_ptr->headers.begin(),
                         request_ptr->headers.end());
  request.referrer = request_ptr->referrer;
  request.credentials_mode = request_ptr->credentials_mode;
  request.cache_mode = request_ptr->cache_mode;
  request.redirect_mode = request_ptr->redirect_mode;
  if (request_ptr->integrity.has_value())
    request.integrity = request_ptr->integrity.value();
  request.keepalive = request_ptr->keepalive;
  if (request_ptr->client_id.has_value())
    request.client_id = request_ptr->client_id.value();
  request.is_reload = request_ptr->is_reload;
  request.is_history_navigation = request_ptr->is_history_navigation;
  return request;
}

}  // namespace mojo
