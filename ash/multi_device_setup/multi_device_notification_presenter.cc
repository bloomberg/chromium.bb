// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/multi_device_setup/multi_device_notification_presenter.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/system_tray_controller.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/components/proximity_auth/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification_types.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace ash {

namespace {

const char kNotifierMultiDevice[] = "ash.multi_device_setup";

}  // namespace

// static
const char MultiDeviceNotificationPresenter::kNotificationId[] =
    "cros_multi_device_setup_notification_id";

// static
std::string
MultiDeviceNotificationPresenter::GetNotificationDescriptionForLogging(
    Status notification_status) {
  switch (notification_status) {
    case Status::kNewUserNotificationVisible:
      return "notification to prompt setup";
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return "notification of switch to new host";
    case Status::kExistingUserNewChromebookNotificationVisible:
      return "notification of new Chromebook added";
    case Status::kNoNotificationVisible:
      return "no notification";
  }
  NOTREACHED();
}

MultiDeviceNotificationPresenter::OpenUiDelegate::~OpenUiDelegate() = default;

void MultiDeviceNotificationPresenter::OpenUiDelegate::
    OpenMultiDeviceSetupUi() {
  // TODO(jordynass): Open WebUI once it is refactored to avoid circular
  // dependcy
}

void MultiDeviceNotificationPresenter::OpenUiDelegate::
    OpenChangeConnectedPhoneSettings() {
  // TODO(jordynass): Open the "Settings/Connected Devices/Change Device"
  // subpage once it has been implemented
}

void MultiDeviceNotificationPresenter::OpenUiDelegate::
    OpenConnectedDevicesSettings() {
  // TODO(jordynass): Open the "Settings/Connected Devices" subpage once it
  // has been implemented
}

// static
MultiDeviceNotificationPresenter::NotificationType
MultiDeviceNotificationPresenter::GetMetricValueForNotification(
    Status notification_status) {
  switch (notification_status) {
    case Status::kNewUserNotificationVisible:
      return kNotificationTypeNewUserPotentialHostExists;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      return kNotificationTypeExistingUserHostSwitched;
    case Status::kExistingUserNewChromebookNotificationVisible:
      return kNotificationTypeExistingUserNewChromebookAdded;
    case Status::kNoNotificationVisible:
      NOTREACHED();
      return kNotificationTypeMax;
  }
}

MultiDeviceNotificationPresenter::MultiDeviceNotificationPresenter(
    message_center::MessageCenter* message_center)
    : message_center_(message_center),
      open_ui_delegate_(std::make_unique<OpenUiDelegate>()),
      weak_ptr_factory_(this) {}

MultiDeviceNotificationPresenter::~MultiDeviceNotificationPresenter() = default;

void MultiDeviceNotificationPresenter::OnPotentialHostExistsForNewUser() {
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_TITLE);
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_NEW_USER_POTENTIAL_HOST_EXISTS_MESSAGE);
  ShowNotification(Status::kNewUserNotificationVisible, title, message);
}

void MultiDeviceNotificationPresenter::
    OnConnectedHostSwitchedForExistingUser() {
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_TITLE);
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_HOST_SWITCHED_MESSAGE);
  ShowNotification(Status::kExistingUserHostSwitchedNotificationVisible, title,
                   message);
}

void MultiDeviceNotificationPresenter::OnNewChromebookAddedForExistingUser() {
  base::string16 title = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROMEBOOK_ADDED_TITLE);
  base::string16 message = l10n_util::GetStringUTF16(
      IDS_ASH_MULTI_DEVICE_SETUP_EXISTING_USER_NEW_CHROMEBOOK_ADDED_MESSAGE);
  ShowNotification(Status::kExistingUserNewChromebookNotificationVisible, title,
                   message);
}

void MultiDeviceNotificationPresenter::RemoveMultiDeviceSetupNotification() {
  notification_status_ = Status::kNoNotificationVisible;
  message_center_->RemoveNotification(kNotificationId,
                                      /* by_user */ false);
}

void MultiDeviceNotificationPresenter::OnNotificationClicked() {
  DCHECK(notification_status_ != Status::kNoNotificationVisible);
  PA_LOG(INFO) << "User clicked "
               << GetNotificationDescriptionForLogging(notification_status_)
               << ".";
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup_NotificationClicked",
                            GetMetricValueForNotification(notification_status_),
                            kNotificationTypeMax);
  switch (notification_status_) {
    case Status::kNewUserNotificationVisible:
      open_ui_delegate_->OpenMultiDeviceSetupUi();
      break;
    case Status::kExistingUserHostSwitchedNotificationVisible:
      open_ui_delegate_->OpenChangeConnectedPhoneSettings();
      break;
    case Status::kExistingUserNewChromebookNotificationVisible:
      open_ui_delegate_->OpenConnectedDevicesSettings();
      break;
    case Status::kNoNotificationVisible:
      NOTREACHED();
  }
  RemoveMultiDeviceSetupNotification();
}

void MultiDeviceNotificationPresenter::ShowNotification(
    const Status notification_status,
    const base::string16& title,
    const base::string16& message) {
  PA_LOG(INFO) << "Showing "
               << GetNotificationDescriptionForLogging(notification_status)
               << ".";
  UMA_HISTOGRAM_ENUMERATION("MultiDeviceSetup_NotificationShown",
                            GetMetricValueForNotification(notification_status),
                            kNotificationTypeMax);
  if (message_center_->FindVisibleNotificationById(kNotificationId)) {
    message_center_->UpdateNotification(kNotificationId,
                                        CreateNotification(title, message));
  } else {
    message_center_->AddNotification(CreateNotification(title, message));
  }
  notification_status_ = notification_status;
}

std::unique_ptr<message_center::Notification>
MultiDeviceNotificationPresenter::CreateNotification(
    const base::string16& title,
    const base::string16& message) {
  return message_center::Notification::CreateSystemNotification(
      message_center::NotificationType::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId, title, message, gfx::Image() /* image */,
      base::string16() /* display_source */, GURL() /* origin_url */,
      message_center::NotifierId(
          message_center::NotifierId::NotifierType::SYSTEM_COMPONENT,
          kNotifierMultiDevice),
      message_center::RichNotificationData(),
      new message_center::HandleNotificationClickDelegate(base::BindRepeating(
          &MultiDeviceNotificationPresenter::OnNotificationClicked,
          weak_ptr_factory_.GetWeakPtr())),
      ash::kNotificationMultiDeviceSetupIcon,
      message_center::SystemNotificationWarningLevel::NORMAL);
}

}  // namespace ash
