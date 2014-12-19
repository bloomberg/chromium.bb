// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_TEST_BLINK_PLATFORM_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_TEST_BLINK_PLATFORM_IMPL_H_

#include "mojo/services/html_viewer/blink_platform_impl.h"

#include "third_party/WebKit/public/platform/WebClipboard.h"
#include "third_party/WebKit/public/platform/WebCookieJar.h"

namespace html_viewer {

class TestBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  TestBlinkPlatformImpl();
  virtual ~TestBlinkPlatformImpl();

 private:
  // blink::Platform methods:
  virtual blink::WebCookieJar* cookieJar();
  virtual blink::WebClipboard* clipboard();

  blink::WebClipboard clipboard_;
  blink::WebCookieJar cookie_jar_;
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_TEST_BLINK_PLATFORM_IMPL_H_
