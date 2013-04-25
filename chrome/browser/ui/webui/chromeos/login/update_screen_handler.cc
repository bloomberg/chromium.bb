// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

UpdateScreenHandler::UpdateScreenHandler()
    : screen_(NULL),
      show_on_init_(false) {
}

UpdateScreenHandler::~UpdateScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

void UpdateScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->AddF("checkingForUpdatesMsg",
                IDS_CHECKING_FOR_UPDATE_MSG, IDS_SHORT_PRODUCT_NAME);
  builder->AddF("installingUpdateDesc",
                IDS_UPDATE_MSG, IDS_SHORT_PRODUCT_NAME);

  builder->Add("updateScreenTitle", IDS_UPDATE_SCREEN_TITLE);
  builder->Add("checkingForUpdates", IDS_CHECKING_FOR_UPDATES);
  builder->Add("downloading", IDS_DOWNLOADING);
  builder->Add("downloadingTimeLeftLong", IDS_DOWNLOADING_TIME_LEFT_LONG);
  builder->Add("downloadingTimeLeftStatusOneHour",
               IDS_DOWNLOADING_TIME_LEFT_STATUS_ONE_HOUR);
  builder->Add("downloadingTimeLeftStatusMinutes",
               IDS_DOWNLOADING_TIME_LEFT_STATUS_MINUTES);
  builder->Add("downloadingTimeLeftSmall", IDS_DOWNLOADING_TIME_LEFT_SMALL);

#if !defined(OFFICIAL_BUILD)
  builder->Add("cancelUpdateHint", IDS_UPDATE_CANCEL);
  builder->Add("cancelledUpdateMessage", IDS_UPDATE_CANCELLED);
#else
  builder->Add("cancelUpdateHint", IDS_EMPTY_STRING);
  builder->Add("cancelledUpdateMessage", IDS_EMPTY_STRING);
#endif
}

void UpdateScreenHandler::Initialize() {
  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void UpdateScreenHandler::SetDelegate(UpdateScreenActor::Delegate* screen) {
  screen_ = screen;
}

void UpdateScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenOobeUpdate, NULL);
#if !defined(OFFICIAL_BUILD)
  CallJS("login.UpdateScreen.enableUpdateCancel");
#endif
}

void UpdateScreenHandler::Hide() {
}

void UpdateScreenHandler::PrepareToShow() {
}

void UpdateScreenHandler::ShowManualRebootInfo() {
  CallJS("cr.ui.Oobe.setUpdateMessage",
         l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED));
}

void UpdateScreenHandler::SetProgress(int progress) {
  CallJS("cr.ui.Oobe.setUpdateProgress", progress);
}

void UpdateScreenHandler::ShowEstimatedTimeLeft(bool visible) {
  CallJS("cr.ui.Oobe.showEstimatedTimeLeft", visible);
}

void UpdateScreenHandler::SetEstimatedTimeLeft(const base::TimeDelta& time) {
  CallJS("cr.ui.Oobe.setEstimatedTimeLeft", time.InSecondsF());
}

void UpdateScreenHandler::ShowProgressMessage(bool visible) {
  CallJS("cr.ui.Oobe.showProgressMessage", visible);
}

void UpdateScreenHandler::SetProgressMessage(ProgressMessage message) {
  scoped_ptr<StringValue> progress_message;
  switch (message) {
    case PROGRESS_MESSAGE_UPDATE_AVAILABLE:
      progress_message.reset(Value::CreateStringValue(
          l10n_util::GetStringUTF16(IDS_UPDATE_AVAILABLE)));
      break;
    case PROGRESS_MESSAGE_INSTALLING_UPDATE:
      progress_message.reset(Value::CreateStringValue(
          l10n_util::GetStringFUTF16(IDS_INSTALLING_UPDATE,
            l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME))));
      break;
    case PROGRESS_MESSAGE_VERIFYING:
      progress_message.reset(Value::CreateStringValue(
          l10n_util::GetStringUTF16(IDS_UPDATE_VERIFYING)));
      break;
    case PROGRESS_MESSAGE_FINALIZING:
      progress_message.reset(Value::CreateStringValue(
          l10n_util::GetStringUTF16(IDS_UPDATE_FINALIZING)));
      break;
    default:
      NOTREACHED();
  };
  if (progress_message.get())
    CallJS("cr.ui.Oobe.setProgressMessage", *progress_message);
}

void UpdateScreenHandler::ShowCurtain(bool visible) {
  CallJS("cr.ui.Oobe.showUpdateCurtain", visible);
}

void UpdateScreenHandler::RegisterMessages() {
#if !defined(OFFICIAL_BUILD)
  AddCallback("cancelUpdate", &UpdateScreenHandler::HandleUpdateCancel);
#endif
}

#if !defined(OFFICIAL_BUILD)
void UpdateScreenHandler::HandleUpdateCancel() {
  screen_->CancelUpdate();
}
#endif

}  // namespace chromeos
