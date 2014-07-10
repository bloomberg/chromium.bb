// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_

#include <string>

#include "base/id_map.h"
#include "content/public/common/push_messaging_status.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/WebPushClient.h"

class GURL;

namespace IPC {
class Message;
}  // namespace IPC

namespace blink {
class WebServiceWorkerProvider;
class WebString;
}  // namespace blink

namespace content {
class PushMessagingDispatcher : public RenderFrameObserver,
                                public blink::WebPushClient {
 public:
  explicit PushMessagingDispatcher(RenderFrame* render_frame);
  virtual ~PushMessagingDispatcher();

 private:
  // RenderFrame::Observer implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // WebPushClient implementation.
  // TODO(mvanouwerkerk): Delete this method once its callers are gone and
  // WebPushClient no longer defines it (as pure virtual).
  virtual void registerPushMessaging(
      const blink::WebString& sender_id,
      blink::WebPushRegistrationCallbacks* callbacks);
  virtual void registerPushMessaging(
      const blink::WebString& sender_id,
      blink::WebPushRegistrationCallbacks* callbacks,
      blink::WebServiceWorkerProvider* service_worker_provider);

  void OnRegisterSuccess(int32 callbacks_id,
                         const GURL& endpoint,
                         const std::string& registration_id);

  void OnRegisterError(int32 callbacks_id, PushMessagingStatus status);

  IDMap<blink::WebPushRegistrationCallbacks, IDMapOwnPointer>
      registration_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_
