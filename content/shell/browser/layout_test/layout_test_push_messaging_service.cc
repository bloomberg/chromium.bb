// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_push_messaging_service.h"

#include "base/logging.h"

namespace content {

LayoutTestPushMessagingService::LayoutTestPushMessagingService() {
}

LayoutTestPushMessagingService::~LayoutTestPushMessagingService() {
}

void LayoutTestPushMessagingService::SetPermission(const GURL& origin,
                                                   bool allowed) {
  permission_map_[origin] = allowed ? blink::WebPushPermissionStatusGranted
                                    : blink::WebPushPermissionStatusDenied;
}

void LayoutTestPushMessagingService::ClearPermissions() {
  permission_map_.clear();
}

GURL LayoutTestPushMessagingService::GetPushEndpoint() {
  return GURL("https://example.com/LayoutTestEndpoint");
}

void LayoutTestPushMessagingService::RegisterFromDocument(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    int renderer_id,
    int render_frame_id,
    bool user_gesture,
    const PushMessagingService::RegisterCallback& callback) {
  RegisterFromWorker(requesting_origin, service_worker_registration_id,
                     sender_id, callback);
}

void LayoutTestPushMessagingService::RegisterFromWorker(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const PushMessagingService::RegisterCallback& callback) {
  if (GetPermissionStatus(requesting_origin, requesting_origin) ==
      blink::WebPushPermissionStatusGranted) {
    callback.Run("layoutTestRegistrationId",
                 PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE);
  } else {
    callback.Run("registration_id", PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
  }
}

blink::WebPushPermissionStatus
LayoutTestPushMessagingService::GetPermissionStatus(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  const auto& it = permission_map_.find(requesting_origin);
  if (it == permission_map_.end())
    return blink::WebPushPermissionStatusDefault;
  return it->second;
}

void LayoutTestPushMessagingService::Unregister(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const UnregisterCallback& callback) {
  callback.Run(PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED);
}

}  // namespace content
