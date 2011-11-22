// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides the embedder's side of random webkit glue functions.

#include "build/build_config.h"

#include <vector>

#include "base/string16.h"
#include "base/string_piece.h"
#include "content/common/socket_stream_dispatcher.h"
#include "content/common/view_messages.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/render_thread_impl.h"
#include "googleurl/src/url_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/websocketstreamhandle_bridge.h"

namespace webkit_glue {

void GetPlugins(bool refresh,
                std::vector<webkit::WebPluginInfo>* plugins) {
  if (!RenderThreadImpl::current()->plugin_refresh_allowed())
    refresh = false;
  RenderThreadImpl::current()->Send(
      new ViewHostMsg_GetPlugins(refresh, plugins));
}

// static factory function
ResourceLoaderBridge* ResourceLoaderBridge::Create(
    const ResourceLoaderBridge::RequestInfo& request_info) {
  return ChildThread::current()->CreateBridge(request_info);
}

// static factory function
WebSocketStreamHandleBridge* WebSocketStreamHandleBridge::Create(
    WebKit::WebSocketStreamHandle* handle,
    WebSocketStreamHandleDelegate* delegate) {
  SocketStreamDispatcher* dispatcher =
      ChildThread::current()->socket_stream_dispatcher();
  return dispatcher->CreateBridge(handle, delegate);
}

string16 GetLocalizedString(int message_id) {
  return content::GetContentClient()->GetLocalizedString(message_id);
}

base::StringPiece GetDataResource(int resource_id) {
  return content::GetContentClient()->GetDataResource(resource_id);
}

}  // namespace webkit_glue
