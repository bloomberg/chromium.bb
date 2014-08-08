// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/consumer_management_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/policy/consumer_management_service.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

ConsumerManagementHandler::ConsumerManagementHandler(
    policy::ConsumerManagementService* management_service)
    : management_service_(management_service) {
}

ConsumerManagementHandler::~ConsumerManagementHandler() {
}

void ConsumerManagementHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "consumerManagementOverlay",
                IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY);

  // Enroll.
  localized_strings->SetString(
      "consumerManagementOverlayEnrollTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY_ENROLL_TITLE));
  localized_strings->SetString(
      "consumerManagementOverlayEnrollMessage",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY_ENROLL_MESSAGE));
  localized_strings->SetString(
      "consumerManagementOverlayEnroll",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY_ENROLL));

  // Unenroll.
  localized_strings->SetString(
      "consumerManagementOverlayUnenrollTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY_UNENROLL_TITLE));
  localized_strings->SetString(
      "consumerManagementOverlayUnenrollMessage",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY_UNENROLL_MESSAGE));
  localized_strings->SetString(
      "consumerManagementOverlayUnenroll",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_CONSUMER_MANAGEMENT_OVERLAY_UNENROLL));
}

void ConsumerManagementHandler::RegisterMessages() {
  // Callback to show keyboard overlay.
  web_ui()->RegisterMessageCallback(
      "enrollConsumerManagement",
      base::Bind(&ConsumerManagementHandler::HandleEnrollConsumerManagement,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "unenrollConsumerManagement",
      base::Bind(&ConsumerManagementHandler::HandleUnenrollConsumerManagement,
                 base::Unretained(this)));
}

void ConsumerManagementHandler::HandleEnrollConsumerManagement(
    const base::ListValue* args) {
  if (!chromeos::UserManager::Get()->IsCurrentUserOwner()) {
    LOG(ERROR) << "Received enrollConsumerManagement, but the current user is "
               << "not the owner.";
    return;
  }

  CHECK(management_service_);
  management_service_->SetEnrollmentState(
      policy::ConsumerManagementService::ENROLLMENT_ENROLLING);
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RequestRestart();
}

void ConsumerManagementHandler::HandleUnenrollConsumerManagement(
    const base::ListValue* args) {
  NOTIMPLEMENTED();
}

}  // namespace options
}  // namespace chromeos
