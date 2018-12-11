// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_
#define CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_types.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_response.mojom.h"

namespace content {

// Represents a request/response pair for a settled Background Fetch fetch.
// Analogous to the following structure in the spec:
// http://wicg.github.io/background-fetch/#backgroundfetchsettledfetch
struct CONTENT_EXPORT BackgroundFetchSettledFetch {
  static blink::mojom::FetchAPIResponsePtr CloneResponse(
      const blink::mojom::FetchAPIResponsePtr& response);
  static blink::mojom::FetchAPIRequestPtr CloneRequest(
      const blink::mojom::FetchAPIRequestPtr& request);
  BackgroundFetchSettledFetch();
  BackgroundFetchSettledFetch(const BackgroundFetchSettledFetch& other);
  BackgroundFetchSettledFetch& operator=(
      const BackgroundFetchSettledFetch& other);
  ~BackgroundFetchSettledFetch();

  blink::mojom::FetchAPIRequestPtr request;
  blink::mojom::FetchAPIResponsePtr response;
};

}  // namespace content

#endif  // CONTENT_COMMON_BACKGROUND_FETCH_BACKGROUND_FETCH_TYPES_H_
