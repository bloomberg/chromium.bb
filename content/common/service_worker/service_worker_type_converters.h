// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
#define CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_

#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/common/service_worker_event_status.mojom.h"

namespace mojo {

template <>
struct CONTENT_EXPORT TypeConverter<content::ServiceWorkerStatusCode,
                                    content::ServiceWorkerEventStatus> {
  static content::ServiceWorkerStatusCode Convert(
      content::ServiceWorkerEventStatus status);
};

}  // namespace

#endif  // CONTENT_COMMON_SERVICE_WORKER_SERVICE_WORKER_TYPE_CONVERTERS_H_
