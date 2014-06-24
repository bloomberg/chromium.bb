// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/consumer_management_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

ConsumerManagementHandler::ConsumerManagementHandler() {
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

void ConsumerManagementHandler::InitializeHandler() {
  // TODO(davidyu): Get the real enrollment status. http://crbug.com/353050.
  SetEnrollmentStatus(false);
}

void ConsumerManagementHandler::HandleEnrollConsumerManagement(
    const base::ListValue* args) {
  StartEnrollment();
  SetEnrollmentStatus(true);
}

void ConsumerManagementHandler::HandleUnenrollConsumerManagement(
    const base::ListValue* args) {
  StartUnenrollment();
  SetEnrollmentStatus(false);
}

void ConsumerManagementHandler::StartEnrollment() {
  NOTIMPLEMENTED();
}

void ConsumerManagementHandler::StartUnenrollment() {
  NOTIMPLEMENTED();
}

void ConsumerManagementHandler::SetEnrollmentStatus(
    bool is_enrolled) {
  base::FundamentalValue is_enrolled_value(is_enrolled);
  web_ui()->CallJavascriptFunction(
      "BrowserOptions.setConsumerManagementEnrollmentStatus",
      is_enrolled_value);
}

}  // namespace options
}  // namespace chromeos
