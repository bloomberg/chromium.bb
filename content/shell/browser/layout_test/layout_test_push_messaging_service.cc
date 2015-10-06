// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_push_messaging_service.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "content/public/browser/permission_type.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

namespace content {

namespace {

// P-256 public key made available to layout tests. Must be 32 bytes.
const uint8_t kTestP256Key[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
  0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};

static_assert(sizeof(kTestP256Key) == 32,
              "The fake public key must have the size of a real public key.");

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

void LayoutTestPushMessagingService::SubscribeFromDocument(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    int renderer_id,
    int render_frame_id,
    bool user_visible,
    const PushMessagingService::RegisterCallback& callback) {
  SubscribeFromWorker(requesting_origin, service_worker_registration_id,
                      sender_id, user_visible, callback);
}

void LayoutTestPushMessagingService::SubscribeFromWorker(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    bool user_visible,
    const PushMessagingService::RegisterCallback& callback) {
  if (GetPermissionStatus(requesting_origin, requesting_origin, user_visible) ==
      blink::WebPushPermissionStatusGranted) {
    std::vector<uint8_t> p256dh(
        kTestP256Key, kTestP256Key + arraysize(kTestP256Key));

    callback.Run("layoutTestRegistrationId", p256dh,
                 PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE);
  } else {
    callback.Run("registration_id", std::vector<uint8_t>(),
                 PUSH_REGISTRATION_STATUS_PERMISSION_DENIED);
  }
}

void LayoutTestPushMessagingService::GetPublicEncryptionKey(
    const GURL& origin,
    int64_t service_worker_registration_id,
    const PublicKeyCallback& callback) {
  std::vector<uint8_t> p256dh(
        kTestP256Key, kTestP256Key + arraysize(kTestP256Key));

  callback.Run(true /* success */, p256dh);
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

void LayoutTestPushMessagingService::Unsubscribe(
    const GURL& requesting_origin,
    int64 service_worker_registration_id,
    const std::string& sender_id,
    const UnregisterCallback& callback) {
  callback.Run(PUSH_UNREGISTRATION_STATUS_SUCCESS_UNREGISTERED);
}

}  // namespace content
