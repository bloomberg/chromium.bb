// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/mojo_blink_platform_impl.h"

#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/html_viewer/webclipboard_impl.h"
#include "mojo/services/html_viewer/webcookiejar_impl.h"
#include "mojo/services/html_viewer/websockethandle_impl.h"
#include "mojo/services/html_viewer/weburlloader_impl.h"

namespace mojo {

MojoBlinkPlatformImpl::MojoBlinkPlatformImpl(ApplicationImpl* app) {
  app->ConnectToService("mojo:network_service", &network_service_);

  CookieStorePtr cookie_store;
  network_service_->GetCookieStore(GetProxy(&cookie_store));
  cookie_jar_.reset(new WebCookieJarImpl(cookie_store.Pass()));

  ClipboardPtr clipboard;
  app->ConnectToService("mojo:clipboard", &clipboard);
  clipboard_.reset(new WebClipboardImpl(clipboard.Pass()));
}

MojoBlinkPlatformImpl::~MojoBlinkPlatformImpl() {
}

blink::WebURLLoader* MojoBlinkPlatformImpl::createURLLoader() {
  return new WebURLLoaderImpl(network_service_.get());
}

blink::WebSocketHandle* MojoBlinkPlatformImpl::createWebSocketHandle() {
  return new WebSocketHandleImpl(network_service_.get());
}

}  // namespace mojo
