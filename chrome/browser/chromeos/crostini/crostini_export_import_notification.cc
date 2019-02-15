// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_export_import_notification.h"

#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/platform_util_chromeos.cc"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace crostini {

namespace {

constexpr char kNotifierCrostiniExportImportOperation[] =
    "crostini.export_operation";

}  // namespace

CrostiniExportImportNotification::CrostiniExportImportNotification(
    Profile* profile,
    ExportImportType type,
    const std::string& notification_id,
    const base::FilePath& path)
    : profile_(profile), type_(type), path_(path), weak_ptr_factory_(this) {
  // Messages.
  switch (type) {
    case ExportImportType::EXPORT:
      message_title_ = IDS_CROSTINI_EXPORT_TITLE;
      message_running_ = IDS_CROSTINI_EXPORT_NOTIFICATION_IN_PROGRESS;
      message_done_ = IDS_CROSTINI_EXPORT_NOTIFICATION_DONE;
      message_failed_ = IDS_CROSTINI_EXPORT_NOTIFICATION_FAILED;
      break;
    case ExportImportType::IMPORT:
      message_title_ = IDS_CROSTINI_IMPORT_TITLE;
      message_running_ = IDS_CROSTINI_IMPORT_NOTIFICATION_IN_PROGRESS;
      message_done_ = IDS_CROSTINI_IMPORT_NOTIFICATION_DONE;
      message_failed_ = IDS_CROSTINI_IMPORT_NOTIFICATION_FAILED;
      break;
    default:
      NOTREACHED();
  }

  message_center::RichNotificationData rich_notification_data;
  rich_notification_data.vector_small_image = &ash::kNotificationLinuxIcon;
  rich_notification_data.accent_color = ash::kSystemNotificationColorNormal;

  notification_ = std::make_unique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_PROGRESS, notification_id,
      base::string16(), base::string16(),
      gfx::Image(),  // icon
      l10n_util::GetStringUTF16(message_title_),
      GURL(),  // origin_url
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 kNotifierCrostiniExportImportOperation),
      rich_notification_data,
      base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
          weak_ptr_factory_.GetWeakPtr()));

  UpdateStatus(Status::RUNNING, 0);
}

CrostiniExportImportNotification::~CrostiniExportImportNotification() = default;

void CrostiniExportImportNotification::UpdateStatus(Status status,
                                                    int progress_percent) {
  status_ = status;
  switch (status) {
    case Status::RUNNING:
      if (closed_) {
        return;
      }
      notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
      notification_->set_progress(progress_percent);
      notification_->set_message(l10n_util::GetStringUTF16(message_running_));
      notification_->set_never_timeout(true);
      break;
    case Status::DONE:
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      notification_->set_message(l10n_util::GetStringUTF16(message_done_));
      notification_->set_never_timeout(false);
      break;
    case Status::FAILED:
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      notification_->set_accent_color(
          ash::kSystemNotificationColorCriticalWarning);
      notification_->set_message(l10n_util::GetStringUTF16(message_failed_));
      notification_->set_never_timeout(false);
      break;
    default:
      NOTREACHED();
  }
  NotificationDisplayService* display_service =
      NotificationDisplayService::GetForProfile(profile_);
  display_service->Display(NotificationHandler::Type::TRANSIENT,
                           *notification_);
}

void CrostiniExportImportNotification::Close(bool by_user) {
  closed_ = true;
}

void CrostiniExportImportNotification::Click(
    const base::Optional<int>& button_index,
    const base::Optional<base::string16>& reply) {
  if (type_ == ExportImportType::EXPORT) {
    platform_util::ShowItemInFolder(profile_, path_);
  }
}

}  // namespace crostini
