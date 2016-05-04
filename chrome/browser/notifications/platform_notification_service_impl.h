// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/permission_status.mojom.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/persistent_notification_status.h"

class NotificationDelegate;
class NotificationDisplayService;

namespace content {
class BrowserContext;
struct NotificationResources;
}

namespace gcm {
class PushMessagingBrowserTest;
}

// The platform notification service is the profile-agnostic entry point through
// which Web Notifications can be controlled.
class PlatformNotificationServiceImpl
    : public content::PlatformNotificationService {
 public:
  // Things you can do to a notification.
  enum NotificationOperation {
    NOTIFICATION_CLICK,
    NOTIFICATION_CLOSE,
    NOTIFICATION_SETTINGS
  };

  // Returns the active instance of the service in the browser process. Safe to
  // be called from any thread.
  static PlatformNotificationServiceImpl* GetInstance();

  // Load the profile corresponding to |profile_id| and perform the
  // |operation| on the given notification once it has been loaded.
  void ProcessPersistentNotificationOperation(
      NotificationOperation operation,
      const std::string& profile_id,
      bool incognito,
      const GURL& origin,
      int64_t persistent_notification_id,
      int action_index);

  // To be called when a persistent notification has been clicked on. The
  // Service Worker associated with the registration will be started if
  // needed, on which the event will be fired. Must be called on the UI thread.
  void OnPersistentNotificationClick(
      content::BrowserContext* browser_context,
      int64_t persistent_notification_id,
      const GURL& origin,
      int action_index);

  // To be called when a persistent notification has been closed. The data
  // associated with the notification has to be pruned from the database in this
  // case, to make sure that it continues to be in sync. Must be called on the
  // UI thread.
  void OnPersistentNotificationClose(content::BrowserContext* browser_context,
                                     int64_t persistent_notification_id,
                                     const GURL& origin,
                                     bool by_user);


  // Open the Notification settings screen when clicking the right button.
  void OpenNotificationSettings(content::BrowserContext* browser_context);

  // content::PlatformNotificationService implementation.
  permissions::mojom::PermissionStatus CheckPermissionOnUIThread(
      content::BrowserContext* browser_context,
      const GURL& origin,
      int render_process_id) override;
  permissions::mojom::PermissionStatus CheckPermissionOnIOThread(
      content::ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) override;
  void DisplayNotification(
      content::BrowserContext* browser_context,
      const GURL& origin,
      const content::PlatformNotificationData& notification_data,
      const content::NotificationResources& notification_resources,
      std::unique_ptr<content::DesktopNotificationDelegate> delegate,
      base::Closure* cancel_callback) override;
  void DisplayPersistentNotification(
      content::BrowserContext* browser_context,
      int64_t persistent_notification_id,
      const GURL& origin,
      const content::PlatformNotificationData& notification_data,
      const content::NotificationResources& notification_resources) override;
  void ClosePersistentNotification(
      content::BrowserContext* browser_context,
      int64_t persistent_notification_id) override;
  bool GetDisplayedPersistentNotifications(
      content::BrowserContext* browser_context,
      std::set<std::string>* displayed_notifications) override;

 private:
  friend struct base::DefaultSingletonTraits<PlatformNotificationServiceImpl>;
  friend class PlatformNotificationServiceBrowserTest;
  friend class PlatformNotificationServiceTest;
  friend class PushMessagingBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           CreateNotificationFromData);
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           DisplayNameForContextMessage);

  PlatformNotificationServiceImpl();
  ~PlatformNotificationServiceImpl() override;

  // Creates a new Web Notification-based Notification object.
  // TODO(peter): |delegate| can be a scoped_refptr, but properly passing this
  // through requires changing a whole lot of Notification constructor calls.
  Notification CreateNotificationFromData(
      Profile* profile,
      const GURL& origin,
      const content::PlatformNotificationData& notification_data,
      const content::NotificationResources& notification_resources,
      NotificationDelegate* delegate) const;

  // Returns a display name for an origin, to be used in the context message
  base::string16 DisplayNameForContextMessage(Profile* profile,
                                              const GURL& origin) const;

  // Returns the notification display service to use.
  // This can be overriden in tests.
  // TODO(miguelg): Remove this method in favor of providing a testing factory
  // to the NotificationDisplayServiceFactory.
  NotificationDisplayService* GetNotificationDisplayService(Profile* profile);

  void SetNotificationDisplayServiceForTesting(
      NotificationDisplayService* service);

  // Mapping between a persistent notification id and the id of the associated
  // message_center::Notification object. Must only be used on the UI thread.
  std::map<int64_t, std::string> persistent_notifications_;

  // Tracks the id of persistent notifications that have been closed
  // programmatically to avoid dispatching close events for them.
  std::unordered_set<int64_t> closed_notifications_;

  // Only set and used for tests, owned by the caller in that case.
  NotificationDisplayService* test_display_service_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationServiceImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
