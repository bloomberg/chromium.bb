// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_

#include <stdint.h>

#include <string>
#include <unordered_set>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "content/public/browser/platform_notification_service.h"
#include "ui/message_center/public/cpp/notification.h"

class GURL;
class Profile;

namespace content {
class BrowserContext;
struct NotificationResources;
}  // namespace content

// The platform notification service is the profile-agnostic entry point through
// which Web Notifications can be controlled.
class PlatformNotificationServiceImpl
    : public content::PlatformNotificationService {
 public:
  // Register profile-specific prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns the active instance of the service in the browser process. Safe to
  // be called from any thread.
  static PlatformNotificationServiceImpl* GetInstance();

  // Returns whether the notification identified by |notification_id| was
  // closed programmatically through ClosePersistentNotification().
  bool WasClosedProgrammatically(const std::string& notification_id);

  // content::PlatformNotificationService implementation.
  void DisplayNotification(
      content::BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      const content::PlatformNotificationData& notification_data,
      const content::NotificationResources& notification_resources) override;
  void DisplayPersistentNotification(
      content::BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const content::PlatformNotificationData& notification_data,
      const content::NotificationResources& notification_resources) override;
  void CloseNotification(content::BrowserContext* browser_context,
                         const std::string& notification_id) override;
  void ClosePersistentNotification(content::BrowserContext* browser_context,
                                   const std::string& notification_id) override;
  void GetDisplayedNotifications(
      content::BrowserContext* browser_context,
      const DisplayedNotificationsCallback& callback) override;
  int64_t ReadNextPersistentNotificationId(
      content::BrowserContext* browser_context) override;
  void RecordNotificationUkmEvent(
      content::BrowserContext* browser_context,
      const content::NotificationDatabaseData& data) override;

  void set_history_query_complete_closure_for_testing(
      base::OnceClosure closure) {
    history_query_complete_closure_for_testing_ = std::move(closure);
  }

 private:
  friend struct base::DefaultSingletonTraits<PlatformNotificationServiceImpl>;
  friend class PersistentNotificationHandlerTest;
  friend class PlatformNotificationServiceBrowserTest;
  friend class PlatformNotificationServiceTest;
  friend class PushMessagingBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           CreateNotificationFromData);
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           DisplayNameForContextMessage);
  FRIEND_TEST_ALL_PREFIXES(PlatformNotificationServiceTest,
                           RecordNotificationUkmEvent);

  PlatformNotificationServiceImpl();
  ~PlatformNotificationServiceImpl() override;

  void OnUrlHistoryQueryComplete(const content::NotificationDatabaseData& data,
                                 bool found_url,
                                 const history::URLRow& url_row,
                                 const history::VisitVector& visits);

  // Creates a new Web Notification-based Notification object. Should only be
  // called when the notification is first shown.
  message_center::Notification CreateNotificationFromData(
      Profile* profile,
      const GURL& origin,
      const std::string& notification_id,
      const content::PlatformNotificationData& notification_data,
      const content::NotificationResources& notification_resources) const;

  // Returns a display name for an origin, to be used in the context message
  base::string16 DisplayNameForContextMessage(Profile* profile,
                                              const GURL& origin) const;

  // Clears |closed_notifications_|. Should only be used for testing purposes.
  void ClearClosedNotificationsForTesting() { closed_notifications_.clear(); }

  // Tracks the id of persistent notifications that have been closed
  // programmatically to avoid dispatching close events for them.
  std::unordered_set<std::string> closed_notifications_;

  // Task tracker used for querying URLs in the history service.
  base::CancelableTaskTracker task_tracker_;

  // Testing-only closure to observe when querying the history service has been
  // completed, and the result of logging UKM can be observed.
  base::OnceClosure history_query_complete_closure_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(PlatformNotificationServiceImpl);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
