// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

FileBrowserNotifications::FileBrowserNotifications(Profile* profile)
    : profile_(profile) {
}

FileBrowserNotifications::~FileBrowserNotifications() {
}

void FileBrowserNotifications::RegisterDevice(const std::string& path) {
  mount_requests_.insert(MountRequestsMap::value_type(path,
                                                      MountRequestsInfo()));
}

void FileBrowserNotifications::UnregisterDevice(const std::string& path) {
  mount_requests_.erase(path);
}

void FileBrowserNotifications::ManageNotificationsOnMountCompleted(
    const std::string& system_path, const std::string& label, bool is_parent,
    bool success, bool is_unsupported) {
  MountRequestsMap::iterator it = mount_requests_.find(system_path);
  if (it == mount_requests_.end())
    return;

  // We have to hide device scanning notification if we haven't done it already.
  if (!it->second.device_notification_hidden) {
    HideNotification(DEVICE, system_path);
    it->second.device_notification_hidden = true;
  }

  // Check if there is fail notification for parent device. If so, disregard it.
  // (parent device contains partition table, which is unmountable).
  if (!is_parent && it->second.fail_notification_shown &&
      !it->second.non_parent_device_failed) {
    HideNotification(DEVICE_FAIL, system_path);
    it->second.fail_notification_shown = false;
  }

  // If notificaiton can't change any more, no need to continue.
  if (it->second.fail_message_finalized)
    return;

  int notification_message_id = 0;

  // Do we have a multi-partition device for which at least one mount failed.
  bool fail_on_multipartition_device =
      success ? it->second.non_parent_device_failed
              : it->second.mount_success_exists ||
                it->second.non_parent_device_failed;

  if (fail_on_multipartition_device) {
    it->second.fail_message_finalized = true;
    notification_message_id =
        label.empty() ? IDS_MULTIPART_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE
                      : IDS_MULTIPART_DEVICE_UNSUPPORTED_MESSAGE;
  } else if (!success) {
    // First device failed.
    if (!is_unsupported) {
      notification_message_id =
          label.empty() ? IDS_DEVICE_UNKNOWN_DEFAULT_MESSAGE
                        : IDS_DEVICE_UNKNOWN_MESSAGE;
    } else {
      notification_message_id =
          label.empty() ? IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE
                        : IDS_DEVICE_UNSUPPORTED_MESSAGE;
    }
  }

  if (success) {
    it->second.mount_success_exists = true;
  } else {
    it->second.non_parent_device_failed |= !is_parent;
  }

  if (notification_message_id == 0)
    return;

  if (it->second.fail_notification_shown) {
    HideNotification(DEVICE_FAIL, system_path);
  } else {
    it->second.fail_notification_shown = true;
  }

  it->second.fail_notifications_count++;

  if (!label.empty()) {
    ShowNotificationWithMessage(DEVICE_FAIL, system_path,
        l10n_util::GetStringFUTF16(notification_message_id,
                                   ASCIIToUTF16(label)));
  } else {
    ShowNotificationWithMessage(DEVICE_FAIL, system_path,
        l10n_util::GetStringUTF16(notification_message_id));
  }
}

void FileBrowserNotifications::ShowNotification(NotificationType type,
                                                const std::string& path) {
  ShowNotificationWithMessage(type, path,
      l10n_util::GetStringUTF16(GetMessageId(type)));
}

void FileBrowserNotifications::ShowNotificationWithMessage(
    NotificationType type, const std::string& path, const string16& message) {
  std::string notification_id;
  CreateNotificationId(type, path, &notification_id);
  chromeos::SystemNotification* notification =
      CreateNotification(notification_id, GetIconId(type), GetTitleId(type));
  if (HasMoreInfoLink(type)) {
    notification->Show(message, GetLinkText(), GetLinkCallback(), false, false);
  } else {
    notification->Show(message, false, false);
  }
}

void FileBrowserNotifications::ShowNotificationDelayed(
    NotificationType type, const std::string& path, base::TimeDelta delay) {
  std::string notification_id;
  CreateNotificationId(type, path, &notification_id);
  CreateNotification(notification_id, GetIconId(type), GetTitleId(type));

  PostDelayedShowNotificationTask(notification_id, type,
                                  l10n_util::GetStringUTF16(GetMessageId(type)),
                                  delay);
}

void FileBrowserNotifications::HideNotification(NotificationType type,
                                                const std::string& path) {
  std::string notification_id;
  CreateNotificationId(type, path, &notification_id);
  NotificationMap::iterator it = notifications_.find(notification_id);
  if (it != notifications_.end()) {
    if (it->second->visible())
      it->second->Hide();
    notifications_.erase(it);
  }
}

void FileBrowserNotifications::HideNotificationDelayed(
    NotificationType type, const std::string& path, base::TimeDelta delay) {
  PostDelayedHideNotificationTask(type, path, delay);
}

void FileBrowserNotifications::PostDelayedShowNotificationTask(
    const std::string& notification_id, NotificationType type,
    const string16&  message, base::TimeDelta delay) {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&ShowNotificationDelayedTask, notification_id, type,
                 message, AsWeakPtr()),
      delay);
}

// static
void FileBrowserNotifications::ShowNotificationDelayedTask(
    const std::string& notification_id, NotificationType type,
    const string16& message,
    base::WeakPtr<FileBrowserNotifications> self) {
  if (!self)
    return;

  NotificationMap::iterator it = self->notifications_.find(notification_id);
  if (it != self->notifications_.end()) {
    if (self->HasMoreInfoLink(type)) {
      it->second->Show(message, self->GetLinkText(), self->GetLinkCallback(),
                       false, false);
    } else {
      it->second->Show(message, false, false);
    }
  }
}

void FileBrowserNotifications::PostDelayedHideNotificationTask(
    NotificationType type, const std::string  path, base::TimeDelta delay) {
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      base::Bind(&HideNotificationDelayedTask, type, path, AsWeakPtr()),
      delay);
}

// static
void FileBrowserNotifications::HideNotificationDelayedTask(
    NotificationType type, const std::string& path,
    base::WeakPtr<FileBrowserNotifications> self) {
  if (self)
    self->HideNotification(type, path);
}

chromeos::SystemNotification* FileBrowserNotifications::CreateNotification(
    const std::string& notification_id, int icon_id, int title_id) {
  if (!profile_) {
    NOTREACHED();
    return NULL;
  }

  NotificationMap::iterator it = notifications_.find(notification_id);
  if (it != notifications_.end())
    return it->second.get();

  chromeos::SystemNotification* notification =
      new chromeos::SystemNotification(
          profile_,
          notification_id,
          icon_id,
          l10n_util::GetStringUTF16(title_id));

  notifications_.insert(NotificationMap::value_type(notification_id,
      linked_ptr<chromeos::SystemNotification>(notification)));

  return notification;
}

void FileBrowserNotifications::CreateNotificationId(NotificationType type,
                                                   const std::string& path,
                                                   std::string* id) {
  switch (type) {
    case(DEVICE):
      *id = "D";
      break;
    case(DEVICE_FAIL):
      *id = "DF";
      break;
    case(FORMAT_START):
      *id = "FS";
      break;
    default:
      *id = "FF";
  }

  if (type == DEVICE_FAIL) {
    MountRequestsMap::const_iterator it = mount_requests_.find(path);
    if (it != mount_requests_.end())
      id->append(base::IntToString(it->second.fail_notifications_count));
  }

  id->append(path);
}

int FileBrowserNotifications::GetIconId(NotificationType type) {
  switch (type) {
    case(DEVICE):
    case(FORMAT_SUCCESS):
    case(FORMAT_START):
      return IDR_PAGEINFO_INFO;
    case(DEVICE_FAIL):
    case(FORMAT_START_FAIL):
    case(FORMAT_FAIL):
      return IDR_PAGEINFO_WARNING_MAJOR;
    default:
      NOTREACHED();
      return 0;
  }
}

int FileBrowserNotifications::GetTitleId(NotificationType type) {
  switch (type) {
    case(DEVICE):
    case(DEVICE_FAIL):
      return IDS_REMOVABLE_DEVICE_DETECTION_TITLE;
    case(FORMAT_START):
      return IDS_FORMATTING_OF_DEVICE_PENDING_TITLE;
    case(FORMAT_START_FAIL):
    case(FORMAT_SUCCESS):
    case(FORMAT_FAIL):
      return IDS_FORMATTING_OF_DEVICE_FINISHED_TITLE;
    default:
      NOTREACHED();
      return 0;
  }
}

int FileBrowserNotifications::GetMessageId(NotificationType type) {
  switch (type) {
    case(DEVICE):
      return IDS_REMOVABLE_DEVICE_SCANNING_MESSAGE;
    case(DEVICE_FAIL):
      return IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE;
    case(FORMAT_FAIL):
      return IDS_FORMATTING_FINISHED_FAILURE_MESSAGE;
    case(FORMAT_SUCCESS):
      return IDS_FORMATTING_FINISHED_SUCCESS_MESSAGE;
    case(FORMAT_START):
      return IDS_FORMATTING_OF_DEVICE_PENDING_MESSAGE;
    case(FORMAT_START_FAIL):
      return IDS_FORMATTING_STARTED_FAILURE_MESSAGE;
    default:
      NOTREACHED();
      return 0;
  }
}

void FileBrowserNotifications::OnLinkClicked(const ListValue* args) {
  // TODO(tbarzic): Show more info page when we'll have one.
  NOTREACHED();
}

bool FileBrowserNotifications::HasMoreInfoLink(NotificationType type) {
  // TODO(tbarzic): Make this return type == DEVICE_FAIL; when more info page
  // gets defined.
  return false;
}

const string16& FileBrowserNotifications::GetLinkText() {
  if (link_text_.empty())
    link_text_ = l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  return link_text_;
}

chromeos::BalloonViewHost::MessageCallback
FileBrowserNotifications::GetLinkCallback() {
  return base::Bind(&FileBrowserNotifications::OnLinkClicked, AsWeakPtr());
}
