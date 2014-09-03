// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_BLINK_URL_REQUEST_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_HTML_VIEWER_BLINK_URL_REQUEST_TYPE_CONVERTERS_H_

#include "mojo/services/public/interfaces/network/url_loader.mojom.h"

namespace blink {
class WebURLRequest;
}

namespace mojo {

template <>
struct TypeConverter<URLRequestPtr, blink::WebURLRequest> {
  static URLRequestPtr Convert(const blink::WebURLRequest& request);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_BLINK_URL_REQUEST_TYPE_CONVERTERS_H_
