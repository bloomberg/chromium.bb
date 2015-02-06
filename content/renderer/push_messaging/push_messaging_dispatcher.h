// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_

#include <string>

#include "base/id_map.h"
#include "content/public/common/push_messaging_status.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushClient.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

class GURL;

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

struct Manifest;

class PushMessagingDispatcher : public RenderFrameObserver,
                                public blink::WebPushClient {
 public:
  explicit PushMessagingDispatcher(RenderFrame* render_frame);
  virtual ~PushMessagingDispatcher();

 private:
  // RenderFrame::Observer implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  // WebPushClient implementation.
  virtual void registerPushMessaging(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebPushRegistrationCallbacks* callbacks);  // override

  void DoRegister(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebPushRegistrationCallbacks* callbacks,
      const Manifest& manifest);

  void OnRegisterFromDocumentSuccess(int32 request_id,
                                     const GURL& endpoint,
                                     const std::string& registration_id);

  void OnRegisterFromDocumentError(int32 request_id,
                                   PushRegistrationStatus status);

  IDMap<blink::WebPushRegistrationCallbacks, IDMapOwnPointer>
      registration_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_DISPATCHER_H_
