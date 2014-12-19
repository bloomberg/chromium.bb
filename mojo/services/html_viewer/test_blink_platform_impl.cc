// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/test_blink_platform_impl.h"

namespace html_viewer {

TestBlinkPlatformImpl::TestBlinkPlatformImpl() {
}

TestBlinkPlatformImpl::~TestBlinkPlatformImpl() {
}

blink::WebCookieJar* BlinkPlatformImpl::cookieJar() {
  return cookie_jar_.get();
}

blink::WebClipboard* BlinkPlatformImpl::clipboard() {
  return clipboard_.get();
}

}  // namespace html_viewer
