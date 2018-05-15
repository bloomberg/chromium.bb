// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/cache_storage/cache_storage_mojom_traits.h"
#include "base/logging.h"
#include "content/public/common/referrer_struct_traits.h"

namespace mojo {

using blink::mojom::CacheStorageError;
using blink::mojom::OperationType;

OperationType
EnumTraits<OperationType, content::CacheStorageCacheOperationType>::ToMojom(
    content::CacheStorageCacheOperationType input) {
  switch (input) {
    case content::CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED:
      return OperationType::kUndefined;
    case content::CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT:
      return OperationType::kPut;
    case content::CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE:
      return OperationType::kDelete;
  }
  NOTREACHED();
  return OperationType::kUndefined;
}

bool EnumTraits<OperationType, content::CacheStorageCacheOperationType>::
    FromMojom(OperationType input,
              content::CacheStorageCacheOperationType* out) {
  switch (input) {
    case OperationType::kUndefined:
      *out = content::CACHE_STORAGE_CACHE_OPERATION_TYPE_UNDEFINED;
      return true;
    case OperationType::kPut:
      *out = content::CACHE_STORAGE_CACHE_OPERATION_TYPE_PUT;
      return true;
    case OperationType::kDelete:
      *out = content::CACHE_STORAGE_CACHE_OPERATION_TYPE_DELETE;
      return true;
  }
  return false;
}

bool StructTraits<blink::mojom::QueryParamsDataView,
                  content::CacheStorageCacheQueryParams>::
    Read(blink::mojom::QueryParamsDataView data,
         content::CacheStorageCacheQueryParams* out) {
  base::Optional<base::string16> cache_name;
  if (!data.ReadCacheName(&cache_name))
    return false;
  out->cache_name = base::NullableString16(std::move(cache_name));
  out->ignore_search = data.ignore_search();
  out->ignore_method = data.ignore_method();
  out->ignore_vary = data.ignore_vary();
  return true;
}

bool StructTraits<blink::mojom::BatchOperationDataView,
                  content::CacheStorageBatchOperation>::
    Read(blink::mojom::BatchOperationDataView data,
         content::CacheStorageBatchOperation* out) {
  base::Optional<content::ServiceWorkerResponse> response;
  base::Optional<content::CacheStorageCacheQueryParams> match_params;
  if (!data.ReadOperationType(&out->operation_type))
    return false;
  if (!data.ReadRequest(&out->request))
    return false;
  if (!data.ReadResponse(&response))
    return false;
  if (!data.ReadMatchParams(&match_params))
    return false;
  if (response)
    out->response = *response;
  if (match_params)
    out->match_params = *match_params;
  return true;
}

}  // namespace mojo
