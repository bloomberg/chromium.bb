// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"

#include "ash/ash_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

int GetIconId(FileBrowserNotifications::NotificationType type) {
  switch (type) {
    case FileBrowserNotifications::DEVICE:
    case FileBrowserNotifications::FORMAT_SUCCESS:
    case FileBrowserNotifications::FORMAT_START:
        return IDR_PAGEINFO_INFO;
    case FileBrowserNotifications::DEVICE_FAIL:
    case FileBrowserNotifications::FORMAT_START_FAIL:
    case FileBrowserNotifications::FORMAT_FAIL:
        return IDR_PAGEINFO_WARNING_MAJOR;
    default:
      NOTREACHED();
      return 0;
  }
}

string16 GetTitle(FileBrowserNotifications::NotificationType type) {
  int id;
  switch (type) {
    case FileBrowserNotifications::DEVICE:
    case FileBrowserNotifications::DEVICE_FAIL:
      id = IDS_REMOVABLE_DEVICE_DETECTION_TITLE;
      break;
    case FileBrowserNotifications::FORMAT_START:
      id = IDS_FORMATTING_OF_DEVICE_PENDING_TITLE;
      break;
    case FileBrowserNotifications::FORMAT_START_FAIL:
    case FileBrowserNotifications::FORMAT_SUCCESS:
    case FileBrowserNotifications::FORMAT_FAIL:
      id = IDS_FORMATTING_OF_DEVICE_FINISHED_TITLE;
      break;
    default:
      NOTREACHED();
      id = 0;
  }
  return l10n_util::GetStringUTF16(id);
}

string16 GetMessage(FileBrowserNotifications::NotificationType type) {
  int id;
  switch (type) {
    case FileBrowserNotifications::DEVICE:
      id = IDS_REMOVABLE_DEVICE_SCANNING_MESSAGE;
      break;
    case FileBrowserNotifications::DEVICE_FAIL:
      id = IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE;
      break;
    case FileBrowserNotifications::FORMAT_FAIL:
      id = IDS_FORMATTING_FINISHED_FAILURE_MESSAGE;
      break;
    case FileBrowserNotifications::FORMAT_SUCCESS:
      id = IDS_FORMATTING_FINISHED_SUCCESS_MESSAGE;
      break;
    case FileBrowserNotifications::FORMAT_START:
      id = IDS_FORMATTING_OF_DEVICE_PENDING_MESSAGE;
      break;
    case FileBrowserNotifications::FORMAT_START_FAIL:
      id = IDS_FORMATTING_STARTED_FAILURE_MESSAGE;
      break;
    default:
      NOTREACHED();
      id = 0;
  }
  return l10n_util::GetStringUTF16(id);
}

}  // namespace

// Simple wrapper class to support ether old chromeos SystemNotifications or
// new ash notifications.
class FileBrowserNotifications::NotificationMessage {
 public:
  class Delegate : public NotificationDelegate {
   public:
    Delegate(const base::WeakPtr<FileBrowserNotifications>& host,
             const std::string& id)
        : host_(host),
          id_(id) {}
    virtual ~Delegate() {}
    virtual void Display() OVERRIDE {}
    virtual void Error() OVERRIDE {}
    virtual void Close(bool by_user) OVERRIDE {
      if (host_)
        host_->HideNotificationById(id_);
    }
    virtual void Click() OVERRIDE {
      // TODO(tbarzic): Show more info page once we have one.
    }
    virtual std::string id() const OVERRIDE { return id_; }
    virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
      return NULL;
    }

   private:
    base::WeakPtr<FileBrowserNotifications> host_;
    std::string id_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  NotificationMessage(FileBrowserNotifications* host,
                      Profile* profile,
                      NotificationType type,
                      const std::string& notification_id,
                      const string16& message)
      : profile_(profile),
        type_(type),
        notification_id_(notification_id),
        message_(message) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
            ash::switches::kAshNotifyDisabled)) {
      const gfx::ImageSkia& icon =
          *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_FILES_APP_ICON);
      notification_id_ = DesktopNotificationService::AddIconNotification(
          GURL(), GetTitle(type_), message, icon,
          new Delegate(host->AsWeakPtr(), notification_id_), profile_);
    } else {
      system_notification_.reset(
          new chromeos::SystemNotification(profile_,
                                           notification_id_,
                                           GetIconId(type_),
                                           GetTitle(type_)));
    }
  }

  ~NotificationMessage() {
    if (system_notification_.get() && system_notification_->visible())
      system_notification_->Hide();
  }

  void Show() {
    if (system_notification_.get())
      system_notification_->Show(message_, false, false);
    // Ash notifications are shown automatically (only) when created.
  }

  void set_message(const string16& message) { message_ = message; }

 private:
  Profile* profile_;
  NotificationType type_;
  std::string notification_id_;
  string16 message_;
  scoped_ptr<chromeos::SystemNotification> system_notification_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMessage);
};

struct FileBrowserNotifications::MountRequestsInfo {
  bool mount_success_exists;
  bool fail_message_finalized;
  bool fail_notification_shown;
  bool non_parent_device_failed;
  bool device_notification_hidden;
  int fail_notifications_count;

  MountRequestsInfo() : mount_success_exists(false),
                        fail_message_finalized(false),
                        fail_notification_shown(false),
                        non_parent_device_failed(false),
                        device_notification_hidden(false),
                        fail_notifications_count(0) {
  }
};

FileBrowserNotifications::FileBrowserNotifications(Profile* profile)
    : profile_(profile) {
}

FileBrowserNotifications::~FileBrowserNotifications() {
  STLDeleteContainerPairSecondPointers(notification_map_.begin(),
                                       notification_map_.end());
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
  ShowNotificationWithMessage(type, path, GetMessage(type));
}

void FileBrowserNotifications::ShowNotificationWithMessage(
    NotificationType type,
    const std::string& path,
    const string16& message) {
  std::string notification_id = CreateNotificationId(type, path);
  hidden_notifications_.erase(notification_id);
  ShowNotificationById(type, notification_id, message);
}

void FileBrowserNotifications::ShowNotificationDelayed(
    NotificationType type,
    const std::string& path,
    base::TimeDelta delay) {
  std::string notification_id = CreateNotificationId(type, path);
  hidden_notifications_.erase(notification_id);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FileBrowserNotifications::ShowNotificationById, AsWeakPtr(),
                 type, notification_id, GetMessage(type)),
      delay);
}

void FileBrowserNotifications::HideNotification(NotificationType type,
                                                const std::string& path) {
  std::string notification_id = CreateNotificationId(type, path);
  HideNotificationById(notification_id);
}

void FileBrowserNotifications::HideNotificationDelayed(
    NotificationType type, const std::string& path, base::TimeDelta delay) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FileBrowserNotifications::HideNotification, AsWeakPtr(),
                 type, path),
      delay);
}

std::string FileBrowserNotifications::CreateNotificationId(
    NotificationType type,
    const std::string& path) {
  std::string id;
  switch (type) {
    case DEVICE:
      id = "D";
      break;
    case DEVICE_FAIL:
      id = "DF";
      break;
    case FORMAT_START:
      id = "FS";
      break;
    default:
      id = "FF";
  }

  if (type == DEVICE_FAIL) {
    MountRequestsMap::const_iterator it = mount_requests_.find(path);
    if (it != mount_requests_.end())
      id.append(base::IntToString(it->second.fail_notifications_count));
  }

  id.append(path);
  return id;
}

FileBrowserNotifications::NotificationMessage*
FileBrowserNotifications::GetNotification(NotificationType type,
                                          const std::string& notification_id,
                                          const string16& message) {
  NotificationMap::iterator iter = notification_map_.find(notification_id);
  if (iter != notification_map_.end()) {
    iter->second->set_message(message);
    return iter->second;
  }
  NotificationMessage* new_message =
      new NotificationMessage(this, profile_, type, notification_id, message);
  notification_map_[notification_id] = new_message;
  return new_message;
}

void FileBrowserNotifications::ShowNotificationById(
    NotificationType type,
    const std::string& notification_id,
    const string16& message) {
  if (hidden_notifications_.find(notification_id) !=
      hidden_notifications_.end()) {
    // Notification was hidden after a delayed show was requested.
    hidden_notifications_.erase(notification_id);
    return;
  }
  NotificationMessage* notification =
      GetNotification(type, notification_id, message);
  notification->Show();
}

void FileBrowserNotifications::HideNotificationById(const std::string& id) {
  NotificationMap::iterator it = notification_map_.find(id);
  if (it != notification_map_.end()) {
    delete it->second;
    notification_map_.erase(it);
  } else {
    // Mark as hidden so it does not get shown from a delayed task.
    hidden_notifications_.insert(id);
  }
}
