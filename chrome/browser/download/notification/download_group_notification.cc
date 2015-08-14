// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_group_notification.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/download_item.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"

namespace {

const char kDownloadNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

const size_t kMaxFilenameWidth = 160;  // in px

base::string16 TruncateFilename(const base::FilePath& filename) {
  return gfx::ElideFilename(filename,
      views::Label().font_list(),
      kMaxFilenameWidth);
}

base::string16 GetStatusString(content::DownloadItem* download) {
  switch (download->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      // "Adding to Chrome..."
      if (download->AllDataSaved() &&
          download_crx_util::IsExtensionDownload(*download)) {
        return l10n_util::GetStringUTF16(
            IDS_DOWNLOAD_STATUS_CRX_INSTALL_RUNNING);
      }

      // "Paused"
      if (download->IsPaused())
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_PROGRESS_PAUSED);

      // "100/120 MB" or "100 MB"
      if (download->GetReceivedBytes() > 0) {
        DownloadItemModel model(download);
        return model.GetProgressSizesString();
      }

      // "Starting..."
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_STARTING);
    case content::DownloadItem::COMPLETE:
      // "Removed" or "Completed"
      if (download->GetFileExternallyRemoved())
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_REMOVED);
      else
        return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_COMPLETED);
    case content::DownloadItem::CANCELLED:
      // "Cancelled"
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED);
    case content::DownloadItem::INTERRUPTED: {
      content::DownloadInterruptReason reason = download->GetLastReason();
      if (reason != content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED) {
        // "Failed - <REASON>"
        DownloadItemModel model(download);
        base::string16 interrupt_reason = model.GetInterruptReasonText();
        return l10n_util::GetStringFUTF16(
            IDS_DOWNLOAD_STATUS_INTERRUPTED, interrupt_reason);
      }

      // Same as DownloadItem::CANCELLED.
      return l10n_util::GetStringUTF16(IDS_DOWNLOAD_STATUS_CANCELLED);
    }
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      break;
  }

  NOTREACHED();
  return base::string16();
}

}  // anonymous namespace

DownloadGroupNotification::DownloadGroupNotification(
    Profile* profile, DownloadNotificationManagerForProfile* manager)
    : profile_(profile) {

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  message_center::RichNotificationData data;
  // Creates the notification instance. |title| and |body| will be overridden
  // by UpdateNotificationData() below.
  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_MULTIPLE,
      GURL(kDownloadNotificationOrigin),  // origin_url
      base::string16(),                   // title
      base::string16(),                   // body
      bundle.GetImageNamed(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kDownloadNotificationNotifierId),
      base::string16(),                    // display_source
      "GROUP",  // tag
      data, watcher()));

  notification_->SetSystemPriority();
  notification_->set_never_timeout(false);

  std::vector<message_center::ButtonInfo> notification_actions;
  message_center::ButtonInfo button_info =
      message_center::ButtonInfo(l10n_util::GetStringUTF16(
          IDS_DOWNLOAD_LINK_SHOW_ALL_DOWNLOADS));
  notification_actions.push_back(button_info);
  notification_->set_buttons(notification_actions);
}

DownloadGroupNotification::~DownloadGroupNotification() {}

bool DownloadGroupNotification::IsPopup() const {
  const ProfileID profile_id =
      NotificationUIManager::GetProfileID(profile_);
  const std::string& notification_id = GetNotificationId();
  const std::string& raw_notification_id = g_browser_process->
     notification_ui_manager()->FindById(notification_id, profile_id)->id();
  auto popup_notifications =
      g_browser_process->message_center()->GetPopupNotifications();
  for (auto notification : popup_notifications) {
    if (notification->id() == raw_notification_id)
      return true;
  }
  return false;
}

void DownloadGroupNotification::OnDownloadUpdated(
    content::DownloadItem* download) {
  if (items_.find(download) != items_.end()) {
    Update();
  }
}

void DownloadGroupNotification::OnDownloadAdded(
    content::DownloadItem* download) {
  if (items_.find(download) == items_.end()) {
    items_.insert(download);
    int inprogress_download_count = 0;
    // If new download is started and there are more than 2 downloads in total,
    // show the group notification.
    for (auto it = items_.begin(); it != items_.end(); it++) {
      if (!(*it)->IsDone() && ++inprogress_download_count >= 2) {
        Show();
        break;
      }
    }
  }
}

void DownloadGroupNotification::OnDownloadRemoved(
    content::DownloadItem* download) {
  // The given |download| may be already free'd.
  if (items_.find(download) != items_.end()) {
    items_.erase(download);
    truncated_filename_cache_.erase(download);
    if (items_.size() <= 1)
      Hide();
  }
}

void DownloadGroupNotification::OnNotificationClose() {
  visible_ = false;

  // When the notification is closed, removes the finished downloads.
  auto it = items_.begin();
  while (it != items_.end()) {
    if ((*it)->GetState() != content::DownloadItem::IN_PROGRESS)
      items_.erase(*(it++));
    else
      it++;
  }
}

void DownloadGroupNotification::OnNotificationClick() {
  OpenDownloads();
}

void DownloadGroupNotification::OnNotificationButtonClick(
    int button_index) {
  DCHECK_EQ(0, button_index);
  OpenDownloads();
}

void DownloadGroupNotification::Hide() {
  hide_next_ = true;
  Update();
}

void DownloadGroupNotification::Show() {
  show_next_ = true;
  Update();
}

void DownloadGroupNotification::Update() {
  if (visible_) {
    if (hide_next_) {
      const ProfileID profile_id =
          NotificationUIManager::GetProfileID(profile_);
      const std::string& notification_id = GetNotificationId();
      g_browser_process->notification_ui_manager()->
          CancelById(notification_id, profile_id);
      visible_ = false;
    } else {
      UpdateNotificationData();
      g_browser_process->notification_ui_manager()->
          Update(*notification_, profile_);
    }
  } else {
    if (show_next_) {
      UpdateNotificationData();
      g_browser_process->notification_ui_manager()->
          Add(*notification_, profile_);
      visible_ = true;
    }
  }
  show_next_ = false;
  hide_next_ = false;
}

void DownloadGroupNotification::UpdateNotificationData() {
  bool all_finished = true;
  std::vector<message_center::NotificationItem> subitems;
  for (auto download : items_) {
    DownloadItemModel model(download);
    auto it = truncated_filename_cache_.find(download);
    auto original_filename = download->GetFileNameToReportUser();
    if (it == truncated_filename_cache_.end() ||
        it->second.truncated_filename.empty() ||
        it->second.original_filename != original_filename) {
      truncated_filename_cache_[download].original_filename = original_filename;
      truncated_filename_cache_[download].truncated_filename =
          TruncateFilename(original_filename);
    }

    // TODO(yoshiki): Use emplace_back when C++11 becomes allowed.
    subitems.push_back(message_center::NotificationItem(
        truncated_filename_cache_[download].truncated_filename,
        GetStatusString(download)));

    if (!download->IsDone())
      all_finished = false;
  }
  notification_->set_items(subitems);

  int title_id = all_finished ? IDS_DOWNLOAD_STATUS_GROUP_DONE_TITLE :
                                IDS_DOWNLOAD_STATUS_GROUP_IN_PROGRESS_TITLE;
  notification_->set_title(l10n_util::GetPluralStringFUTF16(
      title_id, items_.size()));
}

std::string DownloadGroupNotification::GetNotificationId() const {
  return "GROUP";
}

void DownloadGroupNotification::OpenDownloads() {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      profile_, chrome::GetActiveDesktop());
  Browser* browser = browser_displayer.browser();
  DCHECK(browser);

  browser->OpenURL(content::OpenURLParams(
      GURL(chrome::kChromeUIDownloadsURL), content::Referrer(),
      NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false /* is_renderer_initiated */));
}
