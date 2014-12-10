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
#include "third_party/WebKit/public/platform/WebPushPermissionStatus.h"

class GURL;

namespace IPC {
class Message;
}  // namespace IPC

namespace blink {
class WebServiceWorkerProvider;
}  // namespace blink

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
  // TODO(mvanouwerkerk): Delete this when it is no longer called.
  virtual void registerPushMessaging(
      blink::WebPushRegistrationCallbacks* callbacks,
      blink::WebServiceWorkerProvider* service_worker_provider);  // override
  virtual void registerPushMessaging(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebPushRegistrationCallbacks* callbacks);  // override
  // TODO(mvanouwerkerk): Delete once the Push API flows through platform.
  // https://crbug.com/389194
  virtual void getPermissionStatus(
      blink::WebPushPermissionStatusCallback* callback,
      blink::WebServiceWorkerProvider* service_worker_provider);  // override

  // TODO(mvanouwerkerk): Delete this when it is no longer called.
  void DoRegisterOld(blink::WebPushRegistrationCallbacks* callbacks,
                     blink::WebServiceWorkerProvider* service_worker_provider,
                     const Manifest& manifest);

  void DoRegister(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebPushRegistrationCallbacks* callbacks,
      const Manifest& manifest);

  void OnRegisterFromDocumentSuccess(int32 request_id,
                                     const GURL& endpoint,
                                     const std::string& registration_id);

  void OnRegisterFromDocumentError(int32 request_id,
                                   PushRegistrationStatus status);

  // TODO(mvanouwerkerk): Delete once the Push API flows through platform.
  // https://crbug.com/389194
  void OnPermissionStatus(int32 callback_id,
                          blink::WebPushPermissionStatus status);
  // TODO(mvanouwerkerk): Delete once the Push API flows through platform.
  // https://crbug.com/389194
  void OnPermissionStatusFailure(int32 callback_id);

  IDMap<blink::WebPushRegistrationCallbacks, IDMapOwnPointer>
      registration_callbacks_;
  // TODO(mvanouwerkerk): Delete once the Push API flows through platform.
  // https://crbug.com/389194
  IDMap<blink::WebPushPermissionStatusCallback, IDMapOwnPointer>
      permission_check_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingDispatcher);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_DISPATCHER_H_
