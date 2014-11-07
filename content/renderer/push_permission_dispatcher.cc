// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/push_permission_dispatcher.h"

#include "content/common/push_messaging_messages.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"

namespace content {

PushPermissionDispatcher::PushPermissionDispatcher(RenderFrame* render_frame)
    : RenderFrameObserver(render_frame) {
}

PushPermissionDispatcher::~PushPermissionDispatcher() {
}

void PushPermissionDispatcher::RequestPermission(blink::WebCallback* callback) {
  DCHECK(callback);
  int request_id = callbacks_.Add(callback);
  Send(new PushMessagingHostMsg_RequestPermission(
      routing_id(),
      request_id,
      blink::WebUserGestureIndicator::isProcessingUserGesture()));
}

bool PushPermissionDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PushPermissionDispatcher, message)
  IPC_MESSAGE_HANDLER(PushMessagingMsg_RequestPermissionResponse,
                      OnRequestPermissionResponse)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PushPermissionDispatcher::OnRequestPermissionResponse(int32 request_id) {
  blink::WebCallback* callback = callbacks_.Lookup(request_id);
  CHECK(callback);

  callback->run();
  callbacks_.Remove(request_id);
}

}  // namespace content
