// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_PROXY_H_
#define CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_PROXY_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

class GURL;

namespace blink {
enum class ServiceWorkerStatusCode;
}  // namespace blink

namespace content {

class BrowserContext;
struct NotificationDatabaseData;
class PlatformNotificationService;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;

class CONTENT_EXPORT PlatformNotificationServiceProxy {
 public:
  using DisplayResultCallback =
      base::OnceCallback<void(bool /* success */,
                              const std::string& /* notification_id */)>;

  PlatformNotificationServiceProxy(
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      BrowserContext* browser_context);

  ~PlatformNotificationServiceProxy();

  // Displays a notification with |data| and calls |callback| with the result.
  // This will verify against the given |service_worker_context_| if available.
  void DisplayNotification(const NotificationDatabaseData& data,
                           DisplayResultCallback callback);

  // Closes the notification with |notification_id|.
  void CloseNotification(const std::string& notification_id);

  // Schedules a notification trigger for |timestamp|.
  void ScheduleTrigger(base::Time timestamp);

  // Gets the next notification trigger or base::Time::Max if none set. Must be
  // called on the UI thread.
  base::Time GetNextTrigger();

 private:
  // Actually calls |notification_service_| to display the notification after
  // verifying the |service_worker_scope|.
  void DoDisplayNotification(const NotificationDatabaseData& data,
                             const GURL& service_worker_scope,
                             DisplayResultCallback callback);

  // Verifies that the service worker exists and is valid for the given
  // notification origin.
  void VerifyServiceWorkerScope(
      const NotificationDatabaseData& data,
      DisplayResultCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  BrowserContext* browser_context_;
  PlatformNotificationService* notification_service_;

  base::WeakPtrFactory<PlatformNotificationServiceProxy> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationServiceProxy);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_PROXY_H_
