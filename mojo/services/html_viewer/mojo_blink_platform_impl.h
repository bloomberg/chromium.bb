// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_MOJO_BLINK_PLATFORM_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_MOJO_BLINK_PLATFORM_IMPL_H_

#include "mojo/services/html_viewer/blink_platform_impl.h"

#include "mojo/services/network/public/interfaces/network_service.mojom.h"

namespace mojo {
class ApplicationImpl;
}

namespace html_viewer {

class WebClipboardImpl;
class WebCookieJarImpl;

class MojoBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  MojoBlinkPlatformImpl(mojo::ApplicationImpl* app);
  virtual ~MojoBlinkPlatformImpl();

 private:
  // BlinkPlatform
  virtual blink::WebURLLoader* createURLLoader() override;
  virtual blink::WebSocketHandle* createWebSocketHandle() override;

  mojo::NetworkServicePtr network_service_;
  scoped_ptr<WebCookieJarImpl> cookie_jar_;
  scoped_ptr<WebClipboardImpl> clipboard_;
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_MOJO_BLINK_PLATFORM_IMPL_H_
