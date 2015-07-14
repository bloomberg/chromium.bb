// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_

#include <map>
#include <set>

#include "content/public/browser/push_messaging_service.h"
#include "content/public/common/push_messaging_status.h"
#include "third_party/WebKit/public/platform/modules/push_messaging/WebPushPermissionStatus.h"

namespace content {

class LayoutTestPushMessagingService : public PushMessagingService {
 public:
  LayoutTestPushMessagingService();
  ~LayoutTestPushMessagingService() override;

  // PushMessagingService implementation:
  GURL GetPushEndpoint() override;
  void SubscribeFromDocument(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      const std::string& sender_id,
      int renderer_id,
      int render_frame_id,
      bool user_visible,
      const PushMessagingService::RegisterCallback& callback) override;
  void SubscribeFromWorker(
      const GURL& requesting_origin,
      int64_t service_worker_registration_id,
      const std::string& sender_id,
      bool user_visible,
      const PushMessagingService::RegisterCallback& callback) override;
  void GetPublicEncryptionKey(
      const GURL& origin,
      int64_t service_worker_registration_id,
      const PushMessagingService::PublicKeyCallback& callback) override;
  blink::WebPushPermissionStatus GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin,
      bool user_visible) override;
  bool SupportNonVisibleMessages() override;
  void Unsubscribe(const GURL& requesting_origin,
                   int64_t service_worker_registration_id,
                   const std::string& sender_id,
                   const UnregisterCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutTestPushMessagingService);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_
