// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/socket_stream_handle_data.h"

#include "webkit/glue/websocketstreamhandle_impl.h"

using webkit_glue::WebSocketStreamHandleImpl;
using WebKit::WebSocketStreamHandle;

namespace content {

// static
void SocketStreamHandleData::AddToHandle(
    WebSocketStreamHandle* handle, int render_view_id) {
  if (!handle) {
    NOTREACHED();
    return;
  }
  WebSocketStreamHandleImpl* impl =
      static_cast<WebSocketStreamHandleImpl*>(handle);
  impl->SetUserData(handle, new SocketStreamHandleData(render_view_id));
}

// static
const SocketStreamHandleData* SocketStreamHandleData::ForHandle(
    WebSocketStreamHandle* handle) {
  if (!handle)
    return NULL;
  WebSocketStreamHandleImpl* impl =
      static_cast<WebSocketStreamHandleImpl*>(handle);
  return static_cast<SocketStreamHandleData*>(impl->GetUserData(handle));
}

}  // namespace content
