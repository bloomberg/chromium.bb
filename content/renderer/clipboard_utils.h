// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CLIPBOARD_UTILS_H_
#define CONTENT_RENDERER_CLIPBOARD_UTILS_H_

#include <string>

#include "content/common/content_export.h"

namespace WebKit {
class WebString;
class WebURL;
}

namespace content {

CONTENT_EXPORT std::string URLToMarkup(const WebKit::WebURL& url,
                                       const WebKit::WebString& title);

CONTENT_EXPORT std::string URLToImageMarkup(const WebKit::WebURL& url,
                                            const WebKit::WebString& title);

}  // namespace content

#endif  //  CONTENT_RENDERER_CLIPBOARD_UTILS_H_
