// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/service_worker_types_struct_traits.h"

#include "base/logging.h"

namespace mojo {

using content::mojom::ServiceWorkerProviderType;

ServiceWorkerProviderType
EnumTraits<ServiceWorkerProviderType, content::ServiceWorkerProviderType>::
    ToMojom(content::ServiceWorkerProviderType input) {
  switch (input) {
    case content::SERVICE_WORKER_PROVIDER_UNKNOWN:
      return ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_UNKNOWN;
    case content::SERVICE_WORKER_PROVIDER_FOR_WINDOW:
      return ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_FOR_WINDOW;
    case content::SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      return ServiceWorkerProviderType::
          SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER;
    case content::SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
      return ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_FOR_CONTROLLER;
  }

  NOTREACHED();
  return ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_UNKNOWN;
}

bool EnumTraits<ServiceWorkerProviderType, content::ServiceWorkerProviderType>::
    FromMojom(ServiceWorkerProviderType input,
              content::ServiceWorkerProviderType* out) {
  switch (input) {
    case ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_UNKNOWN:
      *out = content::SERVICE_WORKER_PROVIDER_UNKNOWN;
      return true;
    case ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_FOR_WINDOW:
      *out = content::SERVICE_WORKER_PROVIDER_FOR_WINDOW;
      return true;
    case ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER:
      *out = content::SERVICE_WORKER_PROVIDER_FOR_SHARED_WORKER;
      return true;
    case ServiceWorkerProviderType::SERVICE_WORKER_PROVIDER_FOR_CONTROLLER:
      *out = content::SERVICE_WORKER_PROVIDER_FOR_CONTROLLER;
      return true;
  }

  return false;
}

}  // namespace mojo
