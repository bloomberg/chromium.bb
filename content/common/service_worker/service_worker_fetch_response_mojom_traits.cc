// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_fetch_response_mojom_traits.h"

#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/referrer_struct_traits.h"
#include "ipc/ipc_message_utils.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::FetchAPIResponseDataView,
                  content::ServiceWorkerResponse>::
    Read(blink::mojom::FetchAPIResponseDataView data,
         content::ServiceWorkerResponse* out) {
  if (!data.ReadUrlList(&out->url_list) ||
      !data.ReadStatusText(&out->status_text) ||
      !data.ReadResponseType(&out->response_type) ||
      !data.ReadHeaders(&out->headers) || !data.ReadBlobUuid(&out->blob_uuid) ||
      !data.ReadError(&out->error) ||
      !data.ReadResponseTime(&out->response_time) ||
      !data.ReadCacheStorageCacheName(&out->cache_storage_cache_name) ||
      !data.ReadCorsExposedHeaderNames(&out->cors_exposed_header_names)) {
    return false;
  }

  out->status_code = data.status_code();
  out->blob_size = data.blob_size();
  out->is_in_cache_storage = data.is_in_cache_storage();

  if (!out->blob_uuid.empty()) {
    blink::mojom::BlobPtr blob = data.TakeBlob<blink::mojom::BlobPtr>();
    out->blob = base::MakeRefCounted<storage::BlobHandle>(std::move(blob));
  }

  return true;
}

}  // namespace mojo
