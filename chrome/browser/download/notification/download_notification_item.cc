// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/notification/download_notification_item.h"

#include "base/files/file_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_crx_util.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/notification/download_notification_manager.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/notifications/profile_notification.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/mime_util/mime_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"

namespace {

const char kDownloadNotificationNotifierId[] =
    "chrome://downloads/notification/id-notifier";

// Maximum size of preview image. If the image exceeds this size, don't show the
// preview image.
const int64 kMaxImagePreviewSize = 10 * 1024 * 1024;  // 10 MB

std::string ReadNotificationImage(const base::FilePath& file_path) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());

  std::string data;
  bool ret = base::ReadFileToString(file_path, &data);
  if (!ret)
    return std::string();

  DCHECK_LE(data.size(), static_cast<size_t>(kMaxImagePreviewSize));

  return data;
}

}  // anonymous namespace

DownloadNotificationItem::DownloadNotificationItem(
    content::DownloadItem* item,
    DownloadNotificationManagerForProfile* manager)
    : item_(item),
      weak_factory_(this) {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

  message_center::RichNotificationData data;
  // Creates the notification instance. |title| and |body| will be overridden
  // by UpdateNotificationData() below.
  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_PROGRESS,
      GURL(kDownloadNotificationOrigin),  // origin_url
      base::string16(),                   // title
      base::string16(),                   // body
      bundle.GetImageNamed(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING),
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kDownloadNotificationNotifierId),
      base::string16(),                    // display_source
      base::UintToString(item_->GetId()),  // tag
      data, watcher()));

  notification_->set_progress(0);
  notification_->set_never_timeout(false);
}

DownloadNotificationItem::~DownloadNotificationItem() {
  if (image_decode_status_ == IN_PROGRESS)
    ImageDecoder::Cancel(this);
}

void DownloadNotificationItem::OnNotificationClose() {
  visible_ = false;

  if (image_decode_status_ == IN_PROGRESS) {
    image_decode_status_ = NOT_STARTED;
    ImageDecoder::Cancel(this);
  }
}

void DownloadNotificationItem::OnNotificationClick() {
  if (item_->IsDangerous()) {
#if defined(FULL_SAFE_BROWSING)
    DownloadCommands(item_).ExecuteCommand(
        DownloadCommands::LEARN_MORE_SCANNING);
#else
    CloseNotificationByUser();
#endif
    return;
  }

  switch (item_->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      item_->SetOpenWhenComplete(!item_->GetOpenWhenComplete());  // Toggle
      break;
    case content::DownloadItem::CANCELLED:
    case content::DownloadItem::INTERRUPTED:
      GetBrowser()->OpenURL(content::OpenURLParams(
          GURL(chrome::kChromeUIDownloadsURL), content::Referrer(),
          NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          false /* is_renderer_initiated */));
      CloseNotificationByUser();
      break;
    case content::DownloadItem::COMPLETE:
      item_->OpenDownload();
      CloseNotificationByUser();
      break;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
  }
}

void DownloadNotificationItem::OnNotificationButtonClick(int button_index) {
  if (button_index < 0 ||
      static_cast<size_t>(button_index) >= button_actions_->size()) {
    // Out of boundary.
    NOTREACHED();
    return;
  }

  DownloadCommands::Command command = button_actions_->at(button_index);
  if (command != DownloadCommands::PAUSE &&
      command != DownloadCommands::RESUME) {
    CloseNotificationByUser();
  }

  DownloadCommands(item_).ExecuteCommand(command);

  // Shows the notification again after clicking "Keep" on dangerous download.
  if (command == DownloadCommands::KEEP) {
    show_next_ = true;
    Update();
  }
}

// DownloadItem::Observer methods
void DownloadNotificationItem::OnDownloadUpdated(content::DownloadItem* item) {
  DCHECK_EQ(item, item_);

  Update();
}

std::string DownloadNotificationItem::GetNotificationId() const {
  return base::UintToString(item_->GetId());
}

void DownloadNotificationItem::CloseNotificationByNonUser() {
  const std::string& notification_id = watcher()->id();
  const ProfileID profile_id = NotificationUIManager::GetProfileID(profile());

  g_browser_process->notification_ui_manager()->
      CancelById(notification_id, profile_id);
}

void DownloadNotificationItem::CloseNotificationByUser() {
  const std::string& notification_id = watcher()->id();
  const ProfileID profile_id = NotificationUIManager::GetProfileID(profile());
  const std::string notification_id_in_message_center =
      ProfileNotification::GetProfileNotificationId(notification_id,
                                                    profile_id);

  g_browser_process->notification_ui_manager()->
      CancelById(notification_id, profile_id);

  // When the message center is visible, |NotificationUIManager::CancelByID()|
  // delays the close hence the notification is not closed at this time. But
  // from the viewpoint of UX of MessageCenter, we should close it immediately
  // because it's by user action. So, we request closing of it directlly to
  // MessageCenter instance.
  // Note that: this calling has no side-effect even when the message center
  // is not opened.
  g_browser_process->message_center()->RemoveNotification(
      notification_id_in_message_center, true /* by_user */);
}

void DownloadNotificationItem::Update() {
  auto download_state = item_->GetState();

  // When the download is just completed (or interrupted), close the
  // notification once and re-show it immediately so it'll pop up.
  bool popup =
      ((download_state == content::DownloadItem::COMPLETE &&
        previous_download_state_ != content::DownloadItem::COMPLETE) ||
       (download_state == content::DownloadItem::INTERRUPTED &&
        previous_download_state_ != content::DownloadItem::INTERRUPTED));

  if (visible_) {
    UpdateNotificationData(popup ? UPDATE_AND_POPUP : UPDATE);
  } else {
    if (show_next_ || popup) {
      UpdateNotificationData(ADD);
      visible_ = true;
    }
  }

  show_next_ = false;
  previous_download_state_ = item_->GetState();
}

void DownloadNotificationItem::UpdateNotificationData(
    NotificationUpdateType type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DownloadItemModel model(item_);
  DownloadCommands command(item_);

  if (item_->IsDangerous()) {
    notification_->set_type(message_center::NOTIFICATION_TYPE_BASE_FORMAT);
    notification_->set_title(GetTitle());
    notification_->set_message(GetWarningText());

    // Show icon.
    SetNotificationIcon(IDR_DOWNLOAD_NOTIFICATION_MALICIOUS);
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
          SetNotificationIcon(IDR_DOWNLOAD_NOTIFICATION_INCOGNITO);
        } else {
          SetNotificationIcon(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING);
        }
        break;
      case content::DownloadItem::COMPLETE:
        DCHECK(item_->IsDone());

        // Shows a notifiation as progress type once so the visible content will
        // be updated.
        // Note: only progress-type notification's content will be updated
        // immediately when the message center is visible.
        if (type == UPDATE_AND_POPUP) {
          notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        } else {
          notification_->set_type(
              message_center::NOTIFICATION_TYPE_BASE_FORMAT);
        }

        notification_->set_progress(100);

        if (is_off_the_record) {
          // TODO(yoshiki): Replace the tentative image.
          SetNotificationIcon(IDR_DOWNLOAD_NOTIFICATION_INCOGNITO);
        } else {
          SetNotificationIcon(IDR_DOWNLOAD_NOTIFICATION_DOWNLOADING);
        }
        break;
      case content::DownloadItem::CANCELLED:
        // Confgirms that a download is cancelled by user action.
        DCHECK(item_->GetLastReason() ==
                   content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED ||
               item_->GetLastReason() ==
                   content::DOWNLOAD_INTERRUPT_REASON_USER_SHUTDOWN);

        CloseNotificationByUser();
        return;  // Skips the remaining since the notification has closed.
      case content::DownloadItem::INTERRUPTED:
        // Shows a notifiation as progress type once so the visible content will
        // be updated. (same as the case of type = COMPLETE)
        if (type == UPDATE_AND_POPUP) {
          notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
        } else {
          notification_->set_type(
              message_center::NOTIFICATION_TYPE_BASE_FORMAT);
        }

        notification_->set_progress(0);
        SetNotificationIcon(IDR_DOWNLOAD_NOTIFICATION_WARNING);
        break;
      case content::DownloadItem::MAX_DOWNLOAD_STATE:  // sentinel
        NOTREACHED();
    }
  }

  std::vector<message_center::ButtonInfo> notification_actions;
  scoped_ptr<std::vector<DownloadCommands::Command>> actions(
      GetExtraActions().Pass());

  button_actions_.reset(new std::vector<DownloadCommands::Command>);
  for (auto it = actions->begin(); it != actions->end(); it++) {
    button_actions_->push_back(*it);
    message_center::ButtonInfo button_info =
        message_center::ButtonInfo(GetCommandLabel(*it));
    button_info.icon = command.GetCommandIcon(*it);
    notification_actions.push_back(button_info);
  }
  notification_->set_buttons(notification_actions);

  if (type == ADD) {
    g_browser_process->notification_ui_manager()->
        Add(*notification_, profile());
  } else if (type == UPDATE || type == UPDATE_AND_POPUP) {
    g_browser_process->notification_ui_manager()->
        Update(*notification_, profile());

    if (type == UPDATE_AND_POPUP) {
      CloseNotificationByNonUser();
      // Changes the type from PROGRESS to BASE_FORMAT.
      notification_->set_type(message_center::NOTIFICATION_TYPE_BASE_FORMAT);
      g_browser_process->notification_ui_manager()->
          Add(*notification_, profile());
    }
  } else {
    NOTREACHED();
  }

  if (item_->IsDone() && image_decode_status_ == NOT_STARTED) {
    // TODO(yoshiki): Add an UMA to collect statistics of image file sizes.

    if (item_->GetReceivedBytes() > kMaxImagePreviewSize)
      return;

    DCHECK(notification_->image().IsEmpty());

    image_decode_status_ = IN_PROGRESS;

    bool maybe_image = false;
    if (mime_util::IsSupportedImageMimeType(item_->GetMimeType())) {
      maybe_image = true;
    } else {
      std::string mime;
      base::FilePath::StringType extension_with_dot =
          item_->GetTargetFilePath().FinalExtension();
      if (!extension_with_dot.empty() &&
          net::GetWellKnownMimeTypeFromExtension(extension_with_dot.substr(1),
                                                 &mime) &&
          mime_util::IsSupportedImageMimeType(mime)) {
        maybe_image = true;
      }
    }

    if (maybe_image) {
      base::FilePath file_path = item_->GetFullPath();
      base::PostTaskAndReplyWithResult(
          content::BrowserThread::GetBlockingPool(), FROM_HERE,
          base::Bind(&ReadNotificationImage, file_path),
          base::Bind(&DownloadNotificationItem::OnImageLoaded,
                     weak_factory_.GetWeakPtr()));
    }
  }
}

void DownloadNotificationItem::OnDownloadRemoved(content::DownloadItem* item) {
  // The given |item| may be already free'd.
  DCHECK_EQ(item, item_);

  // Removing the notification causes calling |NotificationDelegate::Close()|.
  if (g_browser_process->notification_ui_manager()) {
    g_browser_process->notification_ui_manager()->CancelById(
        watcher()->id(), NotificationUIManager::GetProfileID(profile()));
  }

  item_ = nullptr;
}

void DownloadNotificationItem::SetNotificationIcon(int resource_id) {
  if (image_resource_id_ == resource_id)
    return;
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  image_resource_id_ = resource_id;
  notification_->set_icon(bundle.GetImageNamed(image_resource_id_));
}

void DownloadNotificationItem::OnImageLoaded(const std::string& image_data) {
  if (image_data.empty())
    return;

  // TODO(yoshiki): Set option to reduce the image size to supress memory usage.
  ImageDecoder::Start(this, image_data);
}

void DownloadNotificationItem::OnImageDecoded(const SkBitmap& decoded_image) {
  gfx::Image image = gfx::Image::CreateFrom1xBitmap(decoded_image);
  notification_->set_image(image);
  image_decode_status_ = DONE;
  UpdateNotificationData(UPDATE);
}

void DownloadNotificationItem::OnDecodeImageFailed() {
  DCHECK(notification_->image().IsEmpty());

  image_decode_status_ = FAILED;
  UpdateNotificationData(UPDATE);
}

scoped_ptr<std::vector<DownloadCommands::Command>>
DownloadNotificationItem::GetExtraActions() const {
  scoped_ptr<std::vector<DownloadCommands::Command>> actions(
      new std::vector<DownloadCommands::Command>());

  if (item_->IsDangerous()) {
    actions->push_back(DownloadCommands::DISCARD);
    actions->push_back(DownloadCommands::KEEP);
    return actions.Pass();
  }

  switch (item_->GetState()) {
    case content::DownloadItem::IN_PROGRESS:
      if (!item_->IsPaused())
        actions->push_back(DownloadCommands::PAUSE);
      else
        actions->push_back(DownloadCommands::RESUME);
      actions->push_back(DownloadCommands::CANCEL);
      break;
    case content::DownloadItem::CANCELLED:
    case content::DownloadItem::INTERRUPTED:
      if (item_->CanResume())
        actions->push_back(DownloadCommands::RESUME);
      break;
    case content::DownloadItem::COMPLETE:
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
      break;
    case content::DownloadItem::INTERRUPTED:
      title_text = l10n_util::GetStringFUTF16(
          IDS_DOWNLOAD_STATUS_DOWNLOAD_FAILED_TITLE, file_name);
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
    case DownloadCommands::DISCARD:
      id = IDS_DISCARD_DOWNLOAD;
      break;
    case DownloadCommands::KEEP:
      id = IDS_CONFIRM_DOWNLOAD;
      break;
    case DownloadCommands::CANCEL:
      id = IDS_DOWNLOAD_LINK_CANCEL;
      break;
    case DownloadCommands::ALWAYS_OPEN_TYPE:
    case DownloadCommands::PLATFORM_OPEN:
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

Browser* DownloadNotificationItem::GetBrowser() const {
  chrome::ScopedTabbedBrowserDisplayer browser_displayer(
      profile(), chrome::GetActiveDesktop());
  DCHECK(browser_displayer.browser());
  return browser_displayer.browser();
}

Profile* DownloadNotificationItem::profile() const {
  return Profile::FromBrowserContext(item_->GetBrowserContext());
}
