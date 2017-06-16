// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
#define CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/push_messaging.mojom.h"
#include "content/public/common/push_messaging_status.h"
#include "content/public/renderer/render_frame_observer.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushClient.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

class GURL;

namespace blink {
struct WebPushSubscriptionOptions;
}

namespace content {

struct Manifest;
struct ManifestDebugInfo;
struct PushSubscriptionOptions;

class PushMessagingClient : public RenderFrameObserver,
                            public blink::WebPushClient {
 public:
  explicit PushMessagingClient(RenderFrame* render_frame);
  ~PushMessagingClient() override;

 private:
  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // WebPushClient implementation.
  void Subscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks) override;

  void DidGetManifest(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const blink::WebPushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
      const GURL& manifest_url,
      const Manifest& manifest,
      const ManifestDebugInfo&);

  void DoSubscribe(
      blink::WebServiceWorkerRegistration* service_worker_registration,
      const PushSubscriptionOptions& options,
      bool user_gesture,
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks);

  void DidSubscribe(
      std::unique_ptr<blink::WebPushSubscriptionCallbacks> callbacks,
      content::PushRegistrationStatus status,
      const base::Optional<GURL>& endpoint,
      const base::Optional<content::PushSubscriptionOptions>& options,
      const base::Optional<std::vector<uint8_t>>& p256dh,
      const base::Optional<std::vector<uint8_t>>& auth);

  mojom::PushMessagingPtr push_messaging_manager_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingClient);
};

}  // namespace content

#endif  // CONTENT_RENDERER_PUSH_MESSAGING_PUSH_MESSAGING_CLIENT_H_
