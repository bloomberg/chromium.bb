// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/page_navigator.h"

#include "content/public/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class GURL;

OpenURLParams::OpenURLParams(
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    content::PageTransition transition,
    bool is_renderer_initiated)
    : url(url),
      referrer(referrer),
      disposition(disposition),
      transition(transition),
      is_renderer_initiated(is_renderer_initiated) {
}

OpenURLParams::OpenURLParams()
    : disposition(UNKNOWN),
      transition(content::PageTransitionFromInt(0)),
      is_renderer_initiated(false) {
}

OpenURLParams::~OpenURLParams() {
}

