// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/desktop_notification_service_factory.h"
#include "chrome/browser/notifications/message_center_notification_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service.h"
#include "chrome/browser/notifications/sync_notifier/chrome_notifier_service_factory.h"
#include "chrome/browser/notifications/sync_notifier/welcome_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_util.h"
#include "url/gurl.h"

namespace notifier {
namespace {
void UpdateMessageCenter() {
  NotificationUIManager* notification_ui_manager =
      g_browser_process->notification_ui_manager();
  if (notification_ui_manager->DelegatesToMessageCenter()) {
    MessageCenterNotificationManager* message_center_notification_manager =
        static_cast<MessageCenterNotificationManager*>(notification_ui_manager);

    message_center_notification_manager->EnsureMessageCenterClosed();
  }
}
}  // namespace

WelcomeDelegate::WelcomeDelegate(const std::string& notification_id,
                                 Profile* profile,
                                 const message_center::NotifierId notifier_id,
                                 const GURL& on_click_link)
    : notification_id_(notification_id),
      profile_(profile),
      notifier_id_(notifier_id),
      on_click_link_(on_click_link) {
  DCHECK_EQ(message_center::NotifierId::SYNCED_NOTIFICATION_SERVICE,
            notifier_id.type);
}

WelcomeDelegate::~WelcomeDelegate() {}

void WelcomeDelegate::Display() {}

void WelcomeDelegate::Error() {}

void WelcomeDelegate::Close(bool by_user) {}

bool WelcomeDelegate::HasClickedListener() {
  return on_click_link_.is_valid();
}

void WelcomeDelegate::Click() {
  g_browser_process->notification_ui_manager()->CancelById(notification_id_);

  if (!on_click_link_.is_valid()) {
    // TODO(dewittj): Notifications that remove themselves are currently poorly
    // supported.  We need to make it possible to completely remove a
    // notification
    // while the center is open, and then this section can be removed.
    UpdateMessageCenter();
    return;
  }

  chrome::NavigateParams params(
      profile_, on_click_link_, content::PAGE_TRANSITION_AUTO_BOOKMARK);

  params.disposition = SINGLETON_TAB;
  params.window_action = chrome::NavigateParams::SHOW_WINDOW;
  params.user_gesture = true;

  chrome::Navigate(&params);
}

void WelcomeDelegate::ButtonClick(int button_index) {
  DCHECK_EQ(0, button_index);

  // We take a reference to ourselves to prevent destruction until the end of
  // this method.
  scoped_refptr<WelcomeDelegate> this_ptr(this);

  // WARNING: This line causes the |this| to be released.
  g_browser_process->notification_ui_manager()->CancelById(notification_id_);

  DesktopNotificationService* notification_service =
      DesktopNotificationServiceFactory::GetForProfile(profile_);
  if (notification_service)
    notification_service->SetNotifierEnabled(notifier_id_, false);

  ChromeNotifierService* notifier_service =
      ChromeNotifierServiceFactory::GetForProfile(profile_,
                                                  Profile::EXPLICIT_ACCESS);
  if (notifier_service) {
    notifier_service->OnSyncedNotificationServiceEnabled(notifier_id_.id,
                                                         false);
  }

  // TODO(dewittj): Notifications that remove themselves are currently poorly
  // supported.  We need to make it possible to completely remove a notification
  // while the center is open, and then this section can be removed.
  UpdateMessageCenter();
}

std::string WelcomeDelegate::id() const { return notification_id_; }

content::RenderViewHost* WelcomeDelegate::GetRenderViewHost() const {
  return NULL;
}

}  // namespace notifier
