// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_

#include "base/macros.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification_service.mojom.h"
#include "url/origin.h"

namespace content {

class PlatformNotificationContextImpl;
struct PlatformNotificationData;
class ResourceContext;

// Implementation of the NotificationService used for Web Notifications. Is
// responsible for displaying, updating and reading of both non-persistent
// and persistent notifications. Lives on the IO thread.
class BlinkNotificationServiceImpl : public blink::mojom::NotificationService {
 public:
  BlinkNotificationServiceImpl(
      PlatformNotificationContextImpl* notification_context,
      BrowserContext* browser_context,
      ResourceContext* resource_context,
      int render_process_id,
      const url::Origin& origin,
      mojo::InterfaceRequest<blink::mojom::NotificationService> request);
  ~BlinkNotificationServiceImpl() override;

  // blink::mojom::NotificationService implementation.
  void GetPermissionStatus(GetPermissionStatusCallback callback) override;
  void DisplayNonPersistentNotification(
      const PlatformNotificationData& platform_notification_data) override;

 private:
  // Called when an error is detected on binding_.
  void OnConnectionError();

  // The notification context that owns this service instance.
  PlatformNotificationContextImpl* notification_context_;

  BrowserContext* browser_context_;

  ResourceContext* resource_context_;

  int render_process_id_;

  // The origin that this notification service is communicating with.
  url::Origin origin_;

  mojo::Binding<blink::mojom::NotificationService> binding_;

  DISALLOW_COPY_AND_ASSIGN(BlinkNotificationServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_BLINK_NOTIFICATION_SERVICE_IMPL_H_
