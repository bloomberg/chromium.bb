// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/page_navigator.h"

#include "content/common/page_transition_types.h"
#include "webkit/glue/window_open_disposition.h"

class GURL;

OpenURLParams::OpenURLParams(
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    PageTransition::Type transition)
    : url(url),
      referrer(referrer),
      disposition(disposition),
      transition(transition) {
}

OpenURLParams::OpenURLParams() : disposition(UNKNOWN), transition(0) {
}

OpenURLParams::~OpenURLParams() {
}

