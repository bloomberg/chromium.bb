// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
      source_frame_id(-1),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      is_cross_site_redirect(false) {
}

OpenURLParams::OpenURLParams(
    const GURL& url,
    const Referrer& referrer,
    int64 source_frame_id,
    WindowOpenDisposition disposition,
    PageTransition transition,
    bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      source_frame_id(source_frame_id),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated),
      is_cross_site_redirect(false) {
}

OpenURLParams::OpenURLParams()
    : source_frame_id(-1),
      disposition(UNKNOWN),
      transition(PageTransitionFromInt(0)),
      is_renderer_initiated(false),
      is_cross_site_redirect(false) {
}

OpenURLParams::~OpenURLParams() {
}

}  // namespace content
