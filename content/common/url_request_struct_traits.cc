// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/url_request_struct_traits.h"

#include "base/logging.h"

namespace mojo {

content::mojom::RequestPriority
EnumTraits<content::mojom::RequestPriority, net::RequestPriority>::ToMojom(
    net::RequestPriority priority) {
  switch (priority) {
    case net::THROTTLED:
      return content::mojom::RequestPriority::kThrottled;
    case net::IDLE:
      return content::mojom::RequestPriority::kIdle;
    case net::LOWEST:
      return content::mojom::RequestPriority::kLowest;
    case net::LOW:
      return content::mojom::RequestPriority::kLow;
    case net::MEDIUM:
      return content::mojom::RequestPriority::kMedium;
    case net::HIGHEST:
      return content::mojom::RequestPriority::kHighest;
  }
  NOTREACHED();
  return static_cast<content::mojom::RequestPriority>(priority);
}

bool EnumTraits<content::mojom::RequestPriority, net::RequestPriority>::
    FromMojom(content::mojom::RequestPriority in, net::RequestPriority* out) {
  switch (in) {
    case content::mojom::RequestPriority::kThrottled:
      *out = net::THROTTLED;
      return true;
    case content::mojom::RequestPriority::kIdle:
      *out = net::IDLE;
      return true;
    case content::mojom::RequestPriority::kLowest:
      *out = net::LOWEST;
      return true;
    case content::mojom::RequestPriority::kLow:
      *out = net::LOW;
      return true;
    case content::mojom::RequestPriority::kMedium:
      *out = net::MEDIUM;
      return true;
    case content::mojom::RequestPriority::kHighest:
      *out = net::HIGHEST;
      return true;
  }

  NOTREACHED();
  *out = static_cast<net::RequestPriority>(in);
  return true;
}

}  // namespace mojo
