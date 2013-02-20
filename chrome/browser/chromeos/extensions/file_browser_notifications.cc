// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_browser_notifications.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/extensions/file_manager_util.h"
#include "chrome/browser/notifications/desktop_notification_service.h"
#include "chrome/browser/notifications/notification_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/web_ui_util.h"

namespace {

struct NotificationTypeInfo {
  FileBrowserNotifications::NotificationType type;
  const char* notification_id_prefix;
  int icon_id;
  int title_id;
  int message_id;
};

// Information about notification types.
// The order of notification types in the array must match the order of types in
// NotificationType enum (i.e. the following MUST be satisfied:
// kNotificationTypes[type].type == type).
const NotificationTypeInfo kNotificationTypes[] = {
    {
      FileBrowserNotifications::DEVICE,  // type
      "Device_",  // notification_id_prefix
      IDR_FILES_APP_ICON,  // icon_id
      IDS_REMOVABLE_DEVICE_DETECTION_TITLE,  // title_id
      IDS_REMOVABLE_DEVICE_SCANNING_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::DEVICE_FAIL,  // type
      "DeviceFail_",  // notification_id_prefix
      IDR_FILES_APP_ICON,  // icon_id
      IDS_REMOVABLE_DEVICE_DETECTION_TITLE,  // title_id
      IDS_DEVICE_UNSUPPORTED_DEFAULT_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::DEVICE_HARD_UNPLUG,  // type
      "HardUnplug_",  // notification_id_prefix
      IDR_PAGEINFO_WARNING_MAJOR,  // icon_id
      IDS_REMOVABLE_DEVICE_HARD_UNPLUG_TITLE,  // title_id
      IDS_EXTERNAL_STORAGE_HARD_UNPLUG_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::DEVICE_EXTERNAL_STORAGE_DISABLED,  // type
      "DeviceFail_",  // nottification_id_prefix; same as for DEVICE_FAIL.
      IDR_FILES_APP_ICON,  // icon_id
      IDS_REMOVABLE_DEVICE_DETECTION_TITLE,  // title_id
      IDS_EXTERNAL_STORAGE_DISABLED_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::FORMAT_START,  // type
      "FormatStart_",  // notification_id_prefix
      IDR_FILES_APP_ICON,  // icon_id
      IDS_FORMATTING_OF_DEVICE_PENDING_TITLE,  // title_id
      IDS_FORMATTING_OF_DEVICE_PENDING_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::FORMAT_START_FAIL,  // type
      "FormatComplete_",  // notification_id_prefix
      IDR_FILES_APP_ICON,  // icon_id
      IDS_FORMATTING_OF_DEVICE_FAILED_TITLE,  // title_id
      IDS_FORMATTING_STARTED_FAILURE_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::FORMAT_SUCCESS,  // type
      "FormatComplete_",  // notification_id_prefix
      IDR_FILES_APP_ICON,  // icon_id
      IDS_FORMATTING_OF_DEVICE_FINISHED_TITLE,  // title_id
      IDS_FORMATTING_FINISHED_SUCCESS_MESSAGE  // message_id
    },
    {
      FileBrowserNotifications::FORMAT_FAIL,  // type
      "FormatComplete_",  // notifications_id_prefix
      IDR_FILES_APP_ICON,  // icon_id
      IDS_FORMATTING_OF_DEVICE_FAILED_TITLE,  // title_id
      IDS_FORMATTING_FINISHED_FAILURE_MESSAGE  // message_id
    },
};

int GetIconId(FileBrowserNotifications::NotificationType type) {
  DCHECK_GE(type, 0);
  DCHECK_LT(static_cast<size_t>(type), arraysize(kNotificationTypes));
  DCHECK(kNotificationTypes[type].type == type);

  return kNotificationTypes[type].icon_id;
}

string16 GetTitle(FileBrowserNotifications::NotificationType type) {
  DCHECK_GE(type, 0);
  DCHECK_LT(static_cast<size_t>(type), arraysize(kNotificationTypes));
  DCHECK(kNotificationTypes[type].type == type);

  int id = kNotificationTypes[type].title_id;
  if (id < 0)
    return string16();
  return l10n_util::GetStringUTF16(id);
}

string16 GetMessage(FileBrowserNotifications::NotificationType type) {
  DCHECK_GE(type, 0);
  DCHECK_LT(static_cast<size_t>(type), arraysize(kNotificationTypes));
  DCHECK(kNotificationTypes[type].type == type);

  int id = kNotificationTypes[type].message_id;
  if (id < 0)
    return string16();
  return l10n_util::GetStringUTF16(id);
}

std::string GetNotificationId(FileBrowserNotifications::NotificationType type,
                               const std::string& path) {
  DCHECK_GE(type, 0);
  DCHECK_LT(static_cast<size_t>(type), arraysize(kNotificationTypes));
  DCHECK(kNotificationTypes[type].type == type);

  std::string id_prefix(kNotificationTypes[type].notification_id_prefix);
  return id_prefix.append(path);
}

}  // namespace

// Manages file browser notifications. Generates a desktop notification on
// construction and removes it from the host when closed. Owned by the host.
class FileBrowserNotifications::NotificationMessage {
 public:
  class Delegate : public NotificationDelegate {
   public:
    Delegate(const base::WeakPtr<FileBrowserNotifications>& host,
             const std::string& id)
        : host_(host),
          id_(id) {}
    virtual void Display() OVERRIDE {}
    virtual void Error() OVERRIDE {}
    virtual void Close(bool by_user) OVERRIDE {
      if (host_)
        host_->RemoveNotificationById(id_);
    }
    virtual void Click() OVERRIDE {
      // TODO(tbarzic): Show more info page once we have one.
    }
    virtual std::string id() const OVERRIDE { return id_; }
    virtual content::RenderViewHost* GetRenderViewHost() const OVERRIDE {
      return NULL;
    }

   private:
    virtual ~Delegate() {}

    base::WeakPtr<FileBrowserNotifications> host_;
    std::string id_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  NotificationMessage(FileBrowserNotifications* host,
                      Profile* profile,
                      NotificationType type,
                      const std::string& notification_id,
                      const string16& message)
      : message_(message) {
    const gfx::ImageSkia& icon =
        *ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            GetIconId(type));
    // TODO(mukai): refactor here to invoke NotificationUIManager directly.
    const string16 replace_id = UTF8ToUTF16(notification_id);
    DesktopNotificationService::AddIconNotification(
        file_manager_util::GetFileBrowserExtensionUrl(), GetTitle(type),
        message, icon, replace_id,
        new Delegate(host->AsWeakPtr(), notification_id), profile);
  }

  ~NotificationMessage() {}

  // Used in test.
  string16 message() { return message_; }

 private:
  string16 message_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMessage);
};

struct FileBrowserNotifications::MountRequestsInfo {
  bool mount_success_exists;
  bool fail_message_finalized;
  bool fail_notification_shown;
  bool non_parent_device_failed;
  bool device_notification_hidden;

  MountRequestsInfo() : mount_success_exists(false),
                        fail_message_finalized(false),
                        fail_notification_shown(false),
                        non_parent_device_failed(false),
                        device_notification_hidden(false) {
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

  // If notification can't change any more, no need to continue.
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
  std::string notification_id = GetNotificationId(type, path);
  hidden_notifications_.erase(notification_id);
  ShowNotificationById(type, notification_id, message);
}

void FileBrowserNotifications::ShowNotificationDelayed(
    NotificationType type,
    const std::string& path,
    base::TimeDelta delay) {
  std::string notification_id = GetNotificationId(type, path);
  hidden_notifications_.erase(notification_id);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FileBrowserNotifications::ShowNotificationById, AsWeakPtr(),
                 type, notification_id, GetMessage(type)),
      delay);
}

void FileBrowserNotifications::HideNotification(NotificationType type,
                                                const std::string& path) {
  std::string notification_id = GetNotificationId(type, path);
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
  if (notification_map_.find(notification_id) != notification_map_.end()) {
    // Remove any existing notification with |notification_id|.
    // Will trigger Delegate::Close which will call RemoveNotificationById.
    DesktopNotificationService::RemoveNotification(notification_id);
    DCHECK(notification_map_.find(notification_id) == notification_map_.end());
  }
  // Create a new notification with |notification_id|.
  NotificationMessage* new_message =
      new NotificationMessage(this, profile_, type, notification_id, message);
  notification_map_[notification_id] = new_message;
}

void FileBrowserNotifications::HideNotificationById(
    const std::string& notification_id) {
  NotificationMap::iterator it = notification_map_.find(notification_id);
  if (it != notification_map_.end()) {
    // Will trigger Delegate::Close which will call RemoveNotificationById.
    DesktopNotificationService::RemoveNotification(notification_id);
  } else {
    // Mark as hidden so it does not get shown from a delayed task.
    hidden_notifications_.insert(notification_id);
  }
}

void FileBrowserNotifications::RemoveNotificationById(
    const std::string& notification_id) {
  NotificationMap::iterator it = notification_map_.find(notification_id);
  if (it != notification_map_.end()) {
    NotificationMessage* notification = it->second;
    notification_map_.erase(it);
    delete notification;
  }
}

string16 FileBrowserNotifications::GetNotificationMessageForTest(
    const std::string& id) const {
  NotificationMap::const_iterator it = notification_map_.find(id);
  if (it == notification_map_.end())
    return string16();
  return it->second->message();
}
