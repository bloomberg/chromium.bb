// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_
#define CONTENT_PUBLIC_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "third_party/WebKit/public/platform/WebURL.h"

namespace content {

enum class URLLoaderThrottleProviderType {
  // Used for requests from frames. Please note that the requests could be
  // frame or subresource requests.
  kFrame,
  // Used for requests from workers, including dedicated, shared and service
  // workers.
  kWorker
};

class CONTENT_EXPORT URLLoaderThrottleProvider {
 public:
  virtual ~URLLoaderThrottleProvider() {}

  // For requests from frames and dedicated workers, |render_frame_id| should be
  // set to the corresponding frame. For requests from shared or
  // service workers, |render_frame_id| should be set to MSG_ROUTING_NONE.
  virtual std::vector<std::unique_ptr<URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURL& url,
      ResourceType resource_type) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_
