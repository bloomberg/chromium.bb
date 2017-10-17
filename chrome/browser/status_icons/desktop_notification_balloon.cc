// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/status_icons/desktop_notification_balloon.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/notifier_settings.h"

namespace {

void CloseBalloon(const std::string& id, ProfileID profile_id) {
  // The browser process may have gone away during shutting down, in this case
  // notification_ui_manager() will close the balloon in its destructor.
  if (!g_browser_process)
    return;

  g_browser_process->notification_ui_manager()->CancelById(id, profile_id);
}

// Prefix added to the notification ids.
const char kNotificationPrefix[] = "desktop_notification_balloon.";

// Timeout for automatically dismissing the notification balloon.
const size_t kTimeoutSeconds = 6;

class DummyNotificationDelegate : public message_center::NotificationDelegate {
 public:
  explicit DummyNotificationDelegate(const std::string& id, Profile* profile)
      : id_(id), profile_(profile) {}

  void Display() override {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&CloseBalloon, id_,
                   NotificationUIManager::GetProfileID(profile_)),
        base::TimeDelta::FromSeconds(kTimeoutSeconds));
  }

 private:
  ~DummyNotificationDelegate() override {}

  std::string id_;
  Profile* profile_;
};

}  // anonymous namespace

int DesktopNotificationBalloon::id_count_ = 1;

DesktopNotificationBalloon::DesktopNotificationBalloon() : profile_(NULL) {
}

DesktopNotificationBalloon::~DesktopNotificationBalloon() {
  if (!notification_id_.empty())
    CloseBalloon(notification_id_,
                 NotificationUIManager::GetProfileID(profile_));
}

void DesktopNotificationBalloon::DisplayBalloon(
    const gfx::ImageSkia& icon,
    const base::string16& title,
    const base::string16& contents,
    const message_center::NotifierId& notifier_id) {
  // Allowing IO access is required here to cover the corner case where
  // there is no last used profile and the default one is loaded.
  // IO access won't be required for normal uses.
  Profile* profile;
  {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    profile = ProfileManager::GetLastUsedProfile();
  }

  const std::string notification_id =
      kNotificationPrefix + base::IntToString(id_count_++);
  message_center::Notification notification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title,
      contents, gfx::Image(icon), base::string16(), GURL(), notifier_id,
      message_center::RichNotificationData(),
      new DummyNotificationDelegate(notification_id, profile_));

  g_browser_process->notification_ui_manager()->Add(notification, profile);

  notification_id_ = notification.id();
  profile_ = profile;
}
