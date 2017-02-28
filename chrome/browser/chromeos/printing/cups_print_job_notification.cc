// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/cups_print_job_notification.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/printing/cups_print_job.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_print_job_notification_manager.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"

namespace chromeos {

namespace {

const char kCupsPrintJobNotificationId[] =
    "chrome://settings/printing/cups-print-job-notification";

class CupsPrintJobNotificationDelegate : public NotificationDelegate {
 public:
  explicit CupsPrintJobNotificationDelegate(CupsPrintJobNotification* item)
      : item_(item) {}

  // NotificationDelegate overrides:
  void Close(bool by_user) override {
    if (by_user)
      item_->CloseNotificationByUser();
  }

  bool HasClickedListener() override { return true; }

  void ButtonClick(int button_index) override {
    item_->ClickOnNotificationButton(button_index);
  }

  std::string id() const override { return item_->GetNotificationId(); }

 private:
  ~CupsPrintJobNotificationDelegate() override {}

  CupsPrintJobNotification* item_;

  DISALLOW_COPY_AND_ASSIGN(CupsPrintJobNotificationDelegate);
};

}  // namespace

CupsPrintJobNotification::CupsPrintJobNotification(
    CupsPrintJobNotificationManager* manager,
    CupsPrintJob* print_job,
    Profile* profile)
    : notification_manager_(manager),
      notification_id_(print_job->GetUniqueId()),
      print_job_(print_job),
      delegate_(new CupsPrintJobNotificationDelegate(this)),
      profile_(profile) {
  // Create a notification for the print job. The title, body, icon and buttons
  // of the notification will be updated in UpdateNotification().
  notification_.reset(new Notification(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      base::string16(),  // title
      base::string16(),  // body
      gfx::Image(),      // icon
      message_center::NotifierId(message_center::NotifierId::SYSTEM_COMPONENT,
                                 kCupsPrintJobNotificationId),
      base::string16(),  // display_source
      GURL(kCupsPrintJobNotificationId),
      notification_id_,  // tag
      message_center::RichNotificationData(), delegate_.get()));
  UpdateNotification();
}

CupsPrintJobNotification::~CupsPrintJobNotification() {}

void CupsPrintJobNotification::OnPrintJobStatusUpdated() {
  // After cancellation, ignore all updates.
  if (cancelled_by_user_)
    return;

  UpdateNotification();
}

void CupsPrintJobNotification::CloseNotificationByUser() {
  closed_in_middle_ = true;
  g_browser_process->message_center()->RemoveNotification(GetNotificationId(),
                                                          true /* by_user */);
  if (!print_job_ ||
      print_job_->state() == CupsPrintJob::State::STATE_SUSPENDED) {
    notification_manager_->OnPrintJobNotificationRemoved(this);
  }
}

void CupsPrintJobNotification::ClickOnNotificationButton(int button_index) {
  DCHECK(button_index >= 0 &&
         static_cast<size_t>(button_index) < button_commands_->size());

  CupsPrintJobNotification::ButtonCommand button_command =
      button_commands_->at(button_index);
  CupsPrintJobManager* print_job_manager =
      CupsPrintJobManagerFactory::GetForBrowserContext(profile_);
  const ProfileID profile_id = NotificationUIManager::GetProfileID(profile_);

  switch (button_command) {
    case ButtonCommand::CANCEL_PRINTING:
      if (print_job_manager->CancelPrintJob(print_job_)) {
        // only clean up the nofitication if cancel was successful.
        g_browser_process->notification_ui_manager()->CancelById(
            GetNotificationId(), profile_id);
        cancelled_by_user_ = true;
        notification_manager_->OnPrintJobNotificationRemoved(this);
      }
      break;
    case ButtonCommand::PAUSE_PRINTING:
      print_job_manager->SuspendPrintJob(print_job_);
      break;
    case ButtonCommand::RESUME_PRINTING:
      print_job_manager->ResumePrintJob(print_job_);
      break;
    case ButtonCommand::GET_HELP:
      break;
  }
}

const std::string& CupsPrintJobNotification::GetNotificationId() {
  return notification_id_;
}

void CupsPrintJobNotification::UpdateNotification() {
  UpdateNotificationTitle();
  UpdateNotificationIcon();
  UpdateNotificationBodyMessage();
  UpdateNotificationType();
  UpdateNotificationButtons();

  // |STATE_PAGE_DONE| is special since if the user closes the notification in
  // the middle, which means they're not interested in the printing progress, we
  // should prevent showing the following printing progress.
  if (print_job_->state() == CupsPrintJob::State::STATE_PAGE_DONE) {
    if (closed_in_middle_) {
      // If the notification was closed during the printing, prevent showing the
      // following printing progress.
      g_browser_process->notification_ui_manager()->Update(*notification_,
                                                           profile_);
    } else {
      // If it was not closed, update the notification message directly.
      g_browser_process->notification_ui_manager()->Add(*notification_,
                                                        profile_);
    }
  } else {
    closed_in_middle_ = false;
    // In order to make sure it pop up, we should delete it before readding it.
    const ProfileID profile_id = NotificationUIManager::GetProfileID(profile_);
    g_browser_process->notification_ui_manager()->CancelById(
        GetNotificationId(), profile_id);
    g_browser_process->notification_ui_manager()->Add(*notification_, profile_);
  }

  // |print_job_| will be deleted by CupsPrintJobManager if the job is finished
  // and we are not supposed to get any notification update after that.
  if (print_job_->IsJobFinished())
    print_job_ = nullptr;
}

void CupsPrintJobNotification::UpdateNotificationTitle() {
  notification_->set_title(base::UTF8ToUTF16(print_job_->document_title()));
}

void CupsPrintJobNotification::UpdateNotificationIcon() {
  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
      notification_->set_icon(
          bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_WAITING));
      break;
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      notification_->set_icon(
          bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_PRINTING));
      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      notification_->set_icon(
          bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_DONE));
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_ERROR:
      notification_->set_icon(
          bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_ERROR));
    case CupsPrintJob::State::STATE_NONE:
      break;
  }
}

void CupsPrintJobNotification::UpdateNotificationBodyMessage() {
  base::string16 message;
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_NONE:
      break;
    case CupsPrintJob::State::STATE_WAITING:
      message = l10n_util::GetStringFUTF16(
          IDS_PRINT_JOB_WAITING_NOTIFICATION_MESSAGE,
          base::IntToString16(print_job_->total_page_number()),
          base::UTF8ToUTF16(print_job_->printer().display_name()));
      break;
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      message = l10n_util::GetStringFUTF16(
          IDS_PRINT_JOB_PRINTING_NOTIFICATION_MESSAGE,
          base::IntToString16(print_job_->total_page_number()),
          base::UTF8ToUTF16(print_job_->printer().display_name()));

      break;
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
      message = l10n_util::GetStringFUTF16(
          IDS_PRINT_JOB_DONE_NOTIFICATION_MESSAGE,
          base::IntToString16(print_job_->total_page_number()),
          base::UTF8ToUTF16(print_job_->printer().display_name()));
      break;
    case CupsPrintJob::State::STATE_CANCELLED:
    case CupsPrintJob::State::STATE_ERROR:
      message = l10n_util::GetStringFUTF16(
          IDS_PRINT_JOB_ERROR_NOTIFICATION_MESSAGE,
          base::IntToString16(print_job_->total_page_number()),
          base::UTF8ToUTF16(print_job_->printer().display_name()));
      break;
    default:
      break;
  }
  notification_->set_message(message);
}

void CupsPrintJobNotification::UpdateNotificationType() {
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_SUSPENDED:
    case CupsPrintJob::State::STATE_RESUMED:
      notification_->set_type(message_center::NOTIFICATION_TYPE_PROGRESS);
      notification_->set_progress(print_job_->printed_page_number() * 100 /
                                  print_job_->total_page_number());
      break;
    case CupsPrintJob::State::STATE_NONE:
    case CupsPrintJob::State::STATE_WAITING:
    case CupsPrintJob::State::STATE_DOCUMENT_DONE:
    case CupsPrintJob::State::STATE_ERROR:
    case CupsPrintJob::State::STATE_CANCELLED:
      notification_->set_type(message_center::NOTIFICATION_TYPE_SIMPLE);
      break;
  }
}

void CupsPrintJobNotification::UpdateNotificationButtons() {
  std::vector<message_center::ButtonInfo> buttons;
  button_commands_ = GetButtonCommands();
  for (auto& it : *button_commands_) {
    message_center::ButtonInfo button_info =
        message_center::ButtonInfo(GetButtonLabel(it));
    button_info.icon = GetButtonIcon(it);
    buttons.push_back(button_info);
  }
  notification_->set_buttons(buttons);
}

std::unique_ptr<std::vector<CupsPrintJobNotification::ButtonCommand>>
CupsPrintJobNotification::GetButtonCommands() const {
  std::unique_ptr<std::vector<CupsPrintJobNotification::ButtonCommand>>
      commands(new std::vector<CupsPrintJobNotification::ButtonCommand>());
  switch (print_job_->state()) {
    case CupsPrintJob::State::STATE_WAITING:
      commands->push_back(ButtonCommand::CANCEL_PRINTING);
      break;
    case CupsPrintJob::State::STATE_STARTED:
    case CupsPrintJob::State::STATE_PAGE_DONE:
    case CupsPrintJob::State::STATE_RESUMED:
    case CupsPrintJob::State::STATE_SUSPENDED:
      // TODO(crbug.com/679927): Add PAUSE and RESUME buttons.
      commands->push_back(ButtonCommand::CANCEL_PRINTING);
      break;
    case CupsPrintJob::State::STATE_ERROR:
    case CupsPrintJob::State::STATE_CANCELLED:
      commands->push_back(ButtonCommand::GET_HELP);
      break;
    default:
      break;
  }
  return commands;
}

base::string16 CupsPrintJobNotification::GetButtonLabel(
    ButtonCommand button) const {
  switch (button) {
    case ButtonCommand::CANCEL_PRINTING:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_NOTIFICATION_CANCEL_BUTTON);
    case ButtonCommand::PAUSE_PRINTING:
      return l10n_util::GetStringUTF16(IDS_PRINT_JOB_NOTIFICATION_PAUSE_BUTTON);
    case ButtonCommand::RESUME_PRINTING:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_NOTIFICATION_RESUME_BUTTON);
    case ButtonCommand::GET_HELP:
      return l10n_util::GetStringUTF16(
          IDS_PRINT_JOB_NOTIFICATION_GET_HELP_BUTTON);
  }
  return base::string16();
}

gfx::Image CupsPrintJobNotification::GetButtonIcon(ButtonCommand button) const {
  ResourceBundle& bundle = ResourceBundle::GetSharedInstance();
  gfx::Image icon;
  switch (button) {
    case ButtonCommand::CANCEL_PRINTING:
      icon = bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_CANCEL);
      break;
    case ButtonCommand::PAUSE_PRINTING:
      icon = bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_PAUSE);
      break;
    case ButtonCommand::RESUME_PRINTING:
      icon = bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_PLAY);
      break;
    case ButtonCommand::GET_HELP:
      icon = bundle.GetImageNamed(IDR_PRINT_NOTIFICATION_HELP);
      break;
  }
  return icon;
}

}  // namespace chromeos
