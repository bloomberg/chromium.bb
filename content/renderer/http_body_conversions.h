// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_HTTP_BODY_CONVERSIONS_H_
#define CONTENT_RENDERER_HTTP_BODY_CONVERSIONS_H_

#include "content/common/resource_request_body.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"

namespace content {

// Converts |input| to |output| (coverting under the hood
// WebHTTPBody::Element from the Blink layer into
// ResourceRequestBody::Element from the //content layer).
void ConvertToHttpBodyElement(const blink::WebHTTPBody::Element& input,
                              ResourceRequestBody::Element* output);

// Appends |element| to |http_body| (coverting under the hood
// ResourceRequestBody::Element from the //content layer into
// WebHTTPBody::Element from the Blink layer).
void AppendHttpBodyElement(const ResourceRequestBody::Element& element,
                           blink::WebHTTPBody* http_body);

}  // namespace content

#endif  // CONTENT_RENDERER_HTTP_BODY_CONVERSIONS_H_
