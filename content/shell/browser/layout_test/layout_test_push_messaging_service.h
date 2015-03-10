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

  void SetPermission(const GURL& origin, bool allowed);
  void ClearPermissions();

  // PushMessagingService implementation:
  GURL GetPushEndpoint() override;
  void RegisterFromDocument(
      const GURL& requesting_origin,
      int64 service_worker_registration_id,
      const std::string& sender_id,
      int renderer_id,
      int render_frame_id,
      bool user_gesture,
      const PushMessagingService::RegisterCallback& callback) override;
  void RegisterFromWorker(
      const GURL& requesting_origin,
      int64 service_worker_registration_id,
      const std::string& sender_id,
      const PushMessagingService::RegisterCallback& callback) override;
  blink::WebPushPermissionStatus GetPermissionStatus(
      const GURL& requesting_origin,
      const GURL& embedding_origin) override;
  void Unregister(const GURL& requesting_origin,
                  int64 service_worker_registration_id,
                  const std::string& sender_id,
                  const UnregisterCallback& callback) override;

 private:
  // Map from origin to permission status.
  std::map<GURL, blink::WebPushPermissionStatus> permission_map_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestPushMessagingService);
};

}  // namespace content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PUSH_MESSAGING_SERVICE_H_
