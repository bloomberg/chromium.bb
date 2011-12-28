// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/page_navigator.h"

namespace content {

OpenURLParams::OpenURLParams(
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    PageTransition transition,
    bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated) {
}

OpenURLParams::OpenURLParams()
    : disposition(UNKNOWN),
      transition(PageTransitionFromInt(0)),
      is_renderer_initiated(false) {
}

OpenURLParams::~OpenURLParams() {
}

}  // namespace content
