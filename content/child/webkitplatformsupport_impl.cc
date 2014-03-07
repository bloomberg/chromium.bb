// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "content/child/child_thread.h"
#include "content/child/socket_stream_dispatcher.h"
#include "content/child/webcrypto/webcrypto_impl.h"
#include "content/child/webkitplatformsupport_impl.h"
#include "content/child/websocket_bridge.h"
#include "content/public/common/content_client.h"

namespace content {

WebKitPlatformSupportImpl::WebKitPlatformSupportImpl() {
}

WebKitPlatformSupportImpl::~WebKitPlatformSupportImpl() {
}

base::string16 WebKitPlatformSupportImpl::GetLocalizedString(int message_id) {
  return GetContentClient()->GetLocalizedString(message_id);
}

base::StringPiece WebKitPlatformSupportImpl::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) {
  return GetContentClient()->GetDataResource(resource_id, scale_factor);
}

webkit_glue::ResourceLoaderBridge*
WebKitPlatformSupportImpl::CreateResourceLoader(
    const webkit_glue::ResourceLoaderBridge::RequestInfo& request_info) {
  return ChildThread::current()->CreateBridge(request_info);
}

WebSocketStreamHandleBridge*
WebKitPlatformSupportImpl::CreateWebSocketStreamBridge(
    blink::WebSocketStreamHandle* handle,
    WebSocketStreamHandleDelegate* delegate) {
  SocketStreamDispatcher* dispatcher =
      ChildThread::current()->socket_stream_dispatcher();
  return dispatcher->CreateBridge(handle, delegate);
}

blink::WebSocketHandle* WebKitPlatformSupportImpl::createWebSocketHandle() {
  return new WebSocketBridge;
}

blink::WebCrypto* WebKitPlatformSupportImpl::crypto() {
  if (!web_crypto_)
    web_crypto_.reset(new WebCryptoImpl());
  return web_crypto_.get();
}

}  // namespace content
