// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_item.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_delegate.h"

using message_center::Notification;

namespace {

const char kDownloadNotificationNotifierId[] =
    "chrome://settings/display/notification/id-notifier";
const char kDownloadNotificationIdBase[] =
    "chrome://settings/display/notification/id-";

}  // anonymous namespace

DownloadNotificationItem::NotificationWatcher::NotificationWatcher(
    DownloadNotificationItem* item)
    : item_(item) {
}

DownloadNotificationItem::NotificationWatcher::~NotificationWatcher() {
}

void DownloadNotificationItem::NotificationWatcher::Close(bool by_user) {
  item_->OnNotificationClose(by_user);
}

void DownloadNotificationItem::NotificationWatcher::Click() {
  item_->OnNotificationClick();
}

bool DownloadNotificationItem::NotificationWatcher::HasClickedListener() {
  return true;
}

void DownloadNotificationItem::NotificationWatcher::ButtonClick(
    int button_index) {
  item_->OnNotificationButtonClick(button_index);
}

void DownloadNotificationItem::NotificationWatcher::OnNotificationRemoved(
    const std::string& id,
    bool by_user) {
  if (id != item_->notification_->id())
    return;
  item_->OnNotificationRemoved(by_user);
}

DownloadNotificationItem::DownloadNotificationItem(content::DownloadItem* item,
                                                   Delegate* delegate)
    : openable_(false),
      downloading_(false),
      reshow_after_remove_(false),
      image_resource_id_(0),
      watcher_(new NotificationWatcher(this)),
      item_(item),
      delegate_(delegate) {
  item->AddObserver(this);

  message_center_ = message_center::MessageCenter::Get();
  message_center_->AddObserver(watcher_.get());

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  const base::string16 timeout_message =
      l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CRX_INSTALL_RUNNING);
  const base::string16 message =
      l10n_util::GetStringUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);

  std::string id(kDownloadNotificationIdBase);
  id += base::UintToString(item_->GetId());

  message_center::RichNotificationData data;
  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_PROGRESS, id, message, timeout_message,
      bundle.GetImageNamed(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING),
      base::string16() /* display_source */,
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kDownloadNotificationNotifierId),
      data, watcher_.get()));

  notification_->set_progress(0);
  notification_->set_never_timeout(false);

  UpdateNotificationData();

  scoped_ptr<Notification> notification(new Notification(*notification_));
  message_center_->AddNotification(notification.Pass());
}

DownloadNotificationItem::~DownloadNotificationItem() {
  if (item_)
    item_->RemoveObserver(this);
  message_center_->RemoveObserver(watcher_.get());
}

void DownloadNotificationItem::OnNotificationClose(bool by_user) {
  if (item_->GetState() != content::DownloadItem::IN_PROGRESS) {
    reshow_after_remove_ = false;
  } else {
    bool popup = false;

    const std::string id = notification_->id();
    message_center::NotificationList::PopupNotifications popups =
        message_center_->GetPopupNotifications();
    for (auto it = popups.begin(); it != popups.end(); it++) {
      if ((*it)->id() == id) {
        popup = true;
        break;
      }
    }

    // Reshows the notification in the notification center, if the download is
    // in progress and the notifitation being closed is a popup.
    reshow_after_remove_ = popup;
  }

  // OnNotificationRemoved() will be called soon, just after the notification
  // is removed.
}

void DownloadNotificationItem::OnNotificationRemoved(bool by_user) {
  if (reshow_after_remove_) {
    // Sets the notification as read.
    notification_->set_is_read(true);

    // Reshows the notification.
    scoped_ptr<Notification> notification(new Notification(*notification_));
    message_center_->AddNotification(notification.Pass());
    // Show the reshown notification as a non-popup.
    message_center_->MarkSinglePopupAsShown(notification_->id(), true);

    reshow_after_remove_ = false;
  } else {
    // Cancels the download.
    item_->Cancel(by_user);
    delegate_->OnDownloadRemoved(this);
  }
}

void DownloadNotificationItem::OnNotificationClick() {
  if (openable_) {
    if (item_->IsDone())
      item_->OpenDownload();
    else
      item_->SetOpenWhenComplete(!item_->GetOpenWhenComplete());  // Toggle
  }

  if (item_->IsDone())
    message_center_->RemoveNotification(notification_->id(), true);
}

void DownloadNotificationItem::OnNotificationButtonClick(int button_index) {
  if (button_index < 0 ||
      static_cast<size_t>(button_index) >= button_actions_->size()) {
    // Out of boundary.
    NOTREACHED();
    return;
  }

  DownloadCommands::Command command = button_actions_->at(button_index);
  DownloadCommands(item_).ExecuteCommand(command);
}

// DownloadItem::Observer methods
void DownloadNotificationItem::OnDownloadUpdated(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  UpdateNotificationData();

  // Updates notification.
  scoped_ptr<Notification> notification(new Notification(*notification_));
  std::string id = notification->id();
  message_center_->UpdateNotification(id, notification.Pass());
}

void DownloadNotificationItem::UpdateNotificationData() {
  DownloadItemModel model(item_);
  DownloadCommands command(item_);

  if (!downloading_) {
    if (item_->GetState() == content::DownloadItem::IN_PROGRESS) {
      delegate_->OnDownloadStarted(this);
      downloading_ = true;
    }
  } else {
    if (item_->GetState() != content::DownloadItem::IN_PROGRESS) {
      delegate_->OnDownloadStopped(this);
      downloading_ = false;
    }
  }

  if (item_->IsDangerous()) {
    notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
    notification_->set_title(GetTitle());
    notification_->set_message(GetWarningText());

    // Show icon.
    SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_MALICIOUS);
  } else {
    notification_->set_title(GetTitle());
    notification_->set_message(model.GetStatusText());

    bool is_off_the_record = item_->GetBrowserContext() &&
                             item_->GetBrowserContext()->IsOffTheRecord();

    switch (item_->GetState()) {
      case content::DownloadItem::IN_PROGRESS:
        notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        notification_->set_progress(item_->PercentComplete());
        if (is_off_the_record) {
          // TODO(yoshiki): Replace the tentative image.
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_INCOGNITO);
        } else {
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING);
        }
        break;
      case content::DownloadItem::COMPLETE:
        notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
        if (is_off_the_record) {
          // TODO(yoshiki): Replace the tentative image.
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_INCOGNITO);
        } else {
          SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING);
        }

        // TODO(yoshiki): Popup a notification again.
        break;
      case content::DownloadItem::CANCELLED:
        notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
        SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_WARNING);
        break;
      case content::DownloadItem::INTERRUPTED:
        notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
        SetNotificationImage(IDR_DOWNLOAD_NOTIFICATION_WARNING);

        // TODO(yoshiki): Popup a notification again.
        break;
      case content::DownloadItem::MAX_DOWNLOAD_STATE:  // sentinel
        NOTREACHED();
    }
  }

  std::vector<message_center::ButtonInfo> notification_actions;
  scoped_ptr<std::vector<DownloadCommands::Command>> actions(
      GetPossibleActions().Pass());

  openable_ = false;
  button_actions_.reset(new std::vector<DownloadCommands::Command>);
  for (auto it = actions->begin(); it != actions->end(); it++) {
    if (*it == DownloadCommands::OPEN_WHEN_COMPLETE) {
      openable_ = true;
    } else {
      button_actions_->push_back(*it);
      message_center::ButtonInfo button_info =
          message_center::ButtonInfo(GetCommandLabel(*it));
      button_info.icon = command.GetCommandIcon(*it);
      notification_actions.push_back(button_info);
    }
  }
  notification_->set_buttons(notification_actions);

  if (item_->IsDone()) {
    // TODO(yoshiki): If the downloaded file is an image, show the thumbnail.
  }
}

void DownloadNotificationItem::OnDownloadOpened(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);
  // Do nothing.
}

void DownloadNotificationItem::OnDownloadRemoved(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  // Removing the notification causes calling both |OnNotificationClose()| and
  // |OnNotificationRemoved()|.
  message_center_->RemoveNotification(notification_->id(), false);
}

void DownloadNotificationItem::OnDownloadDestroyed(
    content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  item_ = nullptr;
}

void DownloadNotificationItem::SetNotificationImage(int resource_id) {
  if (image_resource_id_ == resource_id)
    return;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  image_resource_id_ = resource_id;
  notification_->set_icon(bundle.GetImageNamed(image_resource_id_));
}

scoped_ptr<std::vector<DownloadCommands::Command>>
DownloadNotificationItem::GetPossibleActions() const {
  scoped_ptr<std::vector<DownloadCommands::Command>> actions(
      new std::vector<DownloadCommands::Command>());

  if (item_->IsDangerous()) {
    actions->push_back(DownloadCommands::DISCARD);
    actions->push_back(DownloadCommands::KEEP);
    return actions.Pass();
  }

  switch (item_->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      actions->push_back(DownloadCommands::OPEN_WHEN_COMPLETE);
      if (!item_->IsPaused())
        actions->push_back(DownloadCommands::PAUSE);
      else
        actions->push_back(DownloadCommands::RESUME);
      break;
    case content::DownloadItem::CANCELLED:
    case content::DownloadItem::INTERRUPTED:
      actions->push_back(DownloadCommands::RETRY);
      break;
    case content::DownloadItem::COMPLETE:
      actions->push_back(DownloadCommands::OPEN_WHEN_COMPLETE);
      actions->push_back(DownloadCommands::SHOW_IN_FOLDER);
      break;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
  return actions.Pass();
}

base::string16 DownloadNotificationItem::GetTitle() const {
  base::string16 title_text;
  base::string16 file_name =
      item_->GetFileNameToReportUser().LossyDisplayName();
  switch (item_->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_IN_PROGRESS_TITLE, file_name);
      break;
    case content::DownloadItem::COMPLETE:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOADED_TITLE, file_name);
    case content::DownloadItem::INTERRUPTED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOADED_TITLE, file_name);
      break;
    case content::DownloadItem::CANCELLED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE, file_name);
      break;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
  return title_text;
}

base::string16 DownloadNotificationItem::GetCommandLabel(
    DownloadCommands::Command command) const {
  int id = -1;
  switch (command) {
    case DownloadCommands::OPEN_WHEN_COMPLETE:
      if (item_ && !item_->IsDone())
        id = IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE;
      else
        id = IDS_DOWNLOAD_STATUS_OPEN_WHEN_COMPLETE;
      break;
    case DownloadCommands::PAUSE:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_PAUSE;
      break;
    case DownloadCommands::RESUME:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_RESUME;
      break;
    case DownloadCommands::SHOW_IN_FOLDER:
      id = IDS_DOWNLOAD_LINK_SHOW;
      break;
    case DownloadCommands::RETRY:
      // Only for non menu.
      id = IDS_DOWNLOAD_LINK_RETRY;
      break;
    case DownloadCommands::DISCARD:
      id = IDS_DISCARD_DOWNLOAD;
      break;
    case DownloadCommands::KEEP:
      id = IDS_CONFIRM_DOWNLOAD;
      break;
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::PLATFORM_OPEN:
    case DownloadCommands::CANCEL:
    case DownloadCommands::LEARN_MORE_SCANNING:
    case DownloadCommands::LEARN_MORE_INTERRUPTED:
      // Only for menu.
      NOTREACHED();
      return base::string16();
  }
  CHECK(id != -1);
  return l10n_util::GetStringUTF16(id);
}

base::string16 DownloadNotificationItem::GetWarningText() const {
  // Should only be called if IsDangerous().
  DCHECK(item_->IsDangerous());
  base::string16 elided_filename =
      item_->GetFileNameToReportUser().LossyDisplayName();
  switch (item_->GetDangerType()) {
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_URL: {
      return l10n_util::GetStringUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_URL);
    }
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE: {
      if (download_crx_util::IsExtensionDownload(*item_)) {
        return l10n_util::GetStringUTF16(
            IDS_PROMPT_DANGEROUS_DOWNLOAD_EXTENSION);
      } else {
        return l10n_util::GetStringFUTF16(IDS_PROMPT_DANGEROUS_DOWNLOAD,
                                          elided_filename);
      }
    }
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_DANGEROUS_HOST: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_MALICIOUS_DOWNLOAD_CONTENT,
                                        elided_filename);
    }
    case content::DOWNLOAD_DANGER_TYPE_UNCOMMON_CONTENT: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_UNCOMMON_DOWNLOAD_CONTENT,
                                        elided_filename);
    }
    case content::DOWNLOAD_DANGER_TYPE_POTENTIALLY_UNWANTED: {
      return l10n_util::GetStringFUTF16(IDS_PROMPT_DOWNLOAD_CHANGES_SETTINGS,
                                        elided_filename);
    }
    case content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS:
    case content::DOWNLOAD_DANGER_TYPE_MAYBE_DANGEROUS_CONTENT:
    case content::DOWNLOAD_DANGER_TYPE_USER_VALIDATED:
    case content::DOWNLOAD_DANGER_TYPE_MAX: {
      break;
    }
  }
  NOTREACHED();
  return base::string16();
}
