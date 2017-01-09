// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_

#include <stdint.h>

#include <map>
#include <set>

#include "base/macros.h"
#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

namespace content {

struct PushSubscriptionOptions;

class LayoutTestPushMessagingService : public PushMessagingService {
 public:
  LayoutTestPushMessagingService();
  ~LayoutTestPushMessagingService() override;

  // PushMessagingService implementation:
  GURL GetEndpoint(bool standard_protocol) const override;
  void SubscribeFromDocument(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      int renderer_id,
      int render_frame_id,
      const PushSubscriptionOptions& options,
      const PushMessagingService::RegisterCallback& callback) override;
  void SubscribeFromWorker(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      const PushSubscriptionOptions& options,
      const PushMessagingService::RegisterCallback& callback) override;
  void GetEncryptionInfo(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const std::string& sender_id,
      const PushMessagingService::EncryptionInfoCallback& callback) override;
  blink::WebPushPermissionStatus GetPermissionStatus(const GURL& origin,
                                                     bool user_visible)
      override;
  bool SupportNonVisibleMessages() override;
  void Unsubscribe(const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   const UnregisterCallback& callback) override;
  void DidDeleteServiceWorkerRegistration(
      const GURL& origin,
      int64_t service_worker_registration_id) override;

 private:
  int64_t subscribed_service_worker_registration_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestPushMessagingService);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_
