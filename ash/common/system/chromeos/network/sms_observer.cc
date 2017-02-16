// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/network/sms_observer.h"

#include "ash/common/system/system_notifier.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center.h"

using chromeos::NetworkHandler;

namespace ash {

namespace {

// Send the |message| to notification center to display to users. Note that each
// notification will be assigned with different |message_id| as notification id.
void ShowNotification(const base::DictionaryValue* message,
                      const std::string& message_text,
                      const std::string& message_number,
                      int message_id) {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (!message_center)
    return;

  const char kNotificationId[] = "chrome://network/sms";
  std::unique_ptr<message_center::Notification> notification;

  notification = base::MakeUnique<message_center::Notification>(
      message_center::NOTIFICATION_TYPE_SIMPLE,
      kNotificationId + std::to_string(message_id),
      base::ASCIIToUTF16(message_number), base::ASCIIToUTF16(message_text),
      gfx::Image(gfx::CreateVectorIcon(
          ash::kSystemMenuSmsIcon, ash::kMenuIconSize, ash::kMenuIconColor)),
      base::string16(), GURL(),
      message_center::NotifierId(message_center::NotifierId::APPLICATION,
                                 ash::system_notifier::kNotifierSms),
      message_center::RichNotificationData(), nullptr);
  message_center->AddNotification(std::move(notification));
}

}  // namespace

SmsObserver::SmsObserver() {
  // TODO(armansito): SMS could be a special case for cellular that requires a
  // user (perhaps the owner) to be logged in. If that is the case, then an
  // additional check should be done before subscribing for SMS notifications.
  if (NetworkHandler::IsInitialized())
    NetworkHandler::Get()->network_sms_handler()->AddObserver(this);
}

SmsObserver::~SmsObserver() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_sms_handler()->RemoveObserver(this);
  }
}

void SmsObserver::MessageReceived(const base::DictionaryValue& message) {
  std::string message_text;
  if (!message.GetStringWithoutPathExpansion(
          chromeos::NetworkSmsHandler::kTextKey, &message_text)) {
    NET_LOG(ERROR) << "SMS message contains no content.";
    return;
  }
  // TODO(armansito): A message might be due to a special "Message Waiting"
  // state that the message is in. Once SMS handling moves to shill, such
  // messages should be filtered there so that this check becomes unnecessary.
  if (message_text.empty()) {
    NET_LOG(DEBUG) << "SMS has empty content text. Ignoring.";
    return;
  }
  std::string message_number;
  if (!message.GetStringWithoutPathExpansion(
          chromeos::NetworkSmsHandler::kNumberKey, &message_number)) {
    NET_LOG(DEBUG) << "SMS contains no number. Ignoring.";
    return;
  }

  NET_LOG(DEBUG) << "Received SMS from: " << message_number
                 << " with text: " << message_text;
  message_id_++;
  ShowNotification(&message, message_text, message_number, message_id_);
}

}  // namespace ash
