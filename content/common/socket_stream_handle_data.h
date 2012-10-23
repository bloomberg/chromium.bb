// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SOCKET_STREAM_HANDLE_DATA_H_
#define CONTENT_RENDERER_SOCKET_STREAM_HANDLE_DATA_H_

#include "base/supports_user_data.h"
#include "content_export.h"

namespace WebKit {
class WebSocketStreamHandle;
}

namespace content {

// User data stored in each WebSocketStreamHandleImpl.
class SocketStreamHandleData : public base::SupportsUserData::Data {
 public:
  virtual ~SocketStreamHandleData() {}

  int render_view_id() const { return render_view_id_; }

  // Creates SocketStreamHandleData object with |render_view_id| and store it
  // to |handle|.
  static void AddToHandle(
      WebKit::WebSocketStreamHandle* handle, int render_view_id);

  // Retrieves the stored user data from WebKit::WebSocketStreamHandle object.
  // |handle| must actually be a WebSocketStreamHandleImpl object.
  CONTENT_EXPORT static const SocketStreamHandleData* ForHandle(
      WebKit::WebSocketStreamHandle* handle);

 private:
  explicit SocketStreamHandleData(int render_view_id)
      : render_view_id_(render_view_id) {
  }

  int render_view_id_;

  DISALLOW_COPY_AND_ASSIGN(SocketStreamHandleData);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SOCKET_STREAM_HANDLE_DATA_H_
