// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_push_messaging_service.h"

#include "base/callback.h"
#include "base/logging.h"
#include "content/public/browser/permission_type.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

namespace content {

namespace {

blink::WebPushPermissionStatus ToWebPushPermissionStatus(
    PermissionStatus status) {
  switch (status) {
    case PERMISSION_STATUS_GRANTED:
      return blink::WebPushPermissionStatusGranted;
    case PERMISSION_STATUS_DENIED:
      return blink::WebPushPermissionStatusDenied;
    case PERMISSION_STATUS_ASK:
      return blink::WebPushPermissionStatusPrompt;
  }

  NOTREACHED();
  return blink::WebPushPermissionStatusLast;
}

}  // anonymous namespace

LayoutTestPushMessagingService::LayoutTestPushMessagingService() {
}

LayoutTestPushMessagingService::~LayoutTestPushMessagingService() {
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
    bool user_visible,
    const PushMessagingService::RegisterCallback& callback) {
  RegisterFromWorker(requesting_origin, service_worker_registration_id,
                     sender_id, user_visible, callback);
}

void LayoutTestPushMessagingService::RegisterFromWorker(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    bool user_visible,
    const PushMessagingService::RegisterCallback& callback) {
  if (GetPermissionStatus(requesting_origin, requesting_origin, user_visible) ==
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
    const GURL& embedding_origin,
    bool user_visible) {
  return ToWebPushPermissionStatus(LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->GetPermissionStatus(PermissionType::PUSH_MESSAGING,
                            requesting_origin,
                            embedding_origin));
}

bool LayoutTestPushMessagingService::SupportNonVisibleMessages() {
  return false;
}

void LayoutTestPushMessagingService::Unregister(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const UnregisterCallback& callback) {
  callback.Run(PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED);
}

}  // namespace content
