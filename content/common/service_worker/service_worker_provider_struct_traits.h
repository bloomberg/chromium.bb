// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_STRUCT_TRAITS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_STRUCT_TRAITS_H_

#include "content/common/service_worker/service_worker_provider.mojom.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::ServiceWorkerProviderHostInfoDataView,
                    content::ServiceWorkerProviderHostInfo> {
  static int32_t provider_id(
      const content::ServiceWorkerProviderHostInfo& info) {
    return info.provider_id;
  }

  static int32_t route_id(const content::ServiceWorkerProviderHostInfo& info) {
    return info.route_id;
  }

  static content::ServiceWorkerProviderType type(
      const content::ServiceWorkerProviderHostInfo& info) {
    return info.type;
  }

  static bool is_parent_frame_secure(
      const content::ServiceWorkerProviderHostInfo& info) {
    return info.is_parent_frame_secure;
  }

  static bool Read(content::mojom::ServiceWorkerProviderHostInfoDataView in,
                   content::ServiceWorkerProviderHostInfo* out);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_STRUCT_TRAITS_H_
