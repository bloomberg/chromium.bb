// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/html_viewer/mojo_blink_platform_impl.h"

#include "mojo/services/html_viewer/webclipboard_impl.h"
#include "mojo/services/html_viewer/webcookiejar_impl.h"
#include "mojo/services/html_viewer/websockethandle_impl.h"
#include "mojo/services/html_viewer/weburlloader_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"

namespace html_viewer {

MojoBlinkPlatformImpl::MojoBlinkPlatformImpl(mojo::ApplicationImpl* app) {
  app->ConnectToService("mojo:network_service", &network_service_);

  mojo::CookieStorePtr cookie_store;
  network_service_->GetCookieStore(GetProxy(&cookie_store));
  cookie_jar_.reset(new WebCookieJarImpl(cookie_store.Pass()));

  mojo::ClipboardPtr clipboard;
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

}  // namespace html_viewer
