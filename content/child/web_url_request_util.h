// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_TARGET_TYPE_CONVERSION_H_
#define CONTENT_CHILD_TARGET_TYPE_CONVERSION_H_

#include "content/common/content_export.h"
#include "content/public/common/resource_type.h"

namespace blink {
class WebURLRequest;
}

namespace content {

CONTENT_EXPORT ResourceType WebURLRequestToResourceType(
    const blink::WebURLRequest& request);

}  // namespace content

#endif  // CONTENT_CHILD_TARGET_TYPE_CONVERSION_H_
