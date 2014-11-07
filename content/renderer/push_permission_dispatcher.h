// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_PERMISSION_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_PERMISSION_DISPATCHER_H_

#include "base/id_map.h"
#include "content/public/renderer/render_frame_observer.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/platform/WebCallback.h"

namespace content {

// Dispatcher for Push API permission requests.
class PushPermissionDispatcher : public RenderFrameObserver {
 public:
  explicit PushPermissionDispatcher(RenderFrame* render_frame);
  ~PushPermissionDispatcher() override;

  // Requests permission to use the Push API for |origin|. The callback
  // will be invoked when the permission status is available. This class will
  // take ownership of |callback|.
  void RequestPermission(blink::WebCallback* callback);

 private:
  // RenderFrame::Observer implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnRequestPermissionResponse(int32 request_id);

  // Tracks the active permission requests. This class takes ownership of the
  // callback objects.
  IDMap<blink::WebCallback, IDMapOwnPointer> callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushPermissionDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_PERMISSION_DISPATCHER_H_
