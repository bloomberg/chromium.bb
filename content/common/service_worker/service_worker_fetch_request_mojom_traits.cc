// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_fetch_request_mojom_traits.h"

#include "base/logging.h"
#include "content/public/common/referrer_struct_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

using network::mojom::FetchRequestMode;


bool StructTraits<blink::mojom::FetchAPIRequestDataView,
                  content::ServiceWorkerFetchRequest>::
    Read(blink::mojom::FetchAPIRequestDataView data,
         content::ServiceWorkerFetchRequest* out) {
  std::unordered_map<std::string, std::string> headers;
  blink::mojom::SerializedBlobPtr serialized_blob_ptr;
  if (!data.ReadMode(&out->mode) ||
      !data.ReadRequestContextType(&out->request_context_type) ||
      !data.ReadFrameType(&out->frame_type) || !data.ReadUrl(&out->url) ||
      !data.ReadMethod(&out->method) || !data.ReadHeaders(&headers) ||
      !data.ReadBlob(&serialized_blob_ptr) ||
      !data.ReadReferrer(&out->referrer) ||
      !data.ReadCredentialsMode(&out->credentials_mode) ||
      !data.ReadRedirectMode(&out->redirect_mode) ||
      !data.ReadIntegrity(&out->integrity) ||
      !data.ReadClientId(&out->client_id)) {
    return false;
  }

  // content::ServiceWorkerFetchRequest doesn't support request body.
  if (serialized_blob_ptr)
    return false;

  out->is_main_resource_load = data.is_main_resource_load();
  out->headers.insert(headers.begin(), headers.end());
  out->cache_mode = data.cache_mode();
  out->keepalive = data.keepalive();
  out->is_reload = data.is_reload();
  out->is_history_navigation = data.is_history_navigation();
  return true;
}

}  // namespace mojo
