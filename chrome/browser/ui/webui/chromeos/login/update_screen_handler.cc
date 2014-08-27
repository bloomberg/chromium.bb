// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kJsScreenPath[] = "login.UpdateScreen";

}  // namespace

namespace chromeos {

UpdateScreenHandler::UpdateScreenHandler()
    : BaseScreenHandler(kJsScreenPath),
      screen_(NULL),
      show_on_init_(false) {
}

UpdateScreenHandler::~UpdateScreenHandler() {
  if (screen_)
    screen_->OnActorDestroyed(this);
}

void UpdateScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("checkingForUpdatesMsg", IDS_CHECKING_FOR_UPDATE_MSG);
  builder->Add("installingUpdateDesc", IDS_UPDATE_MSG);
  builder->Add("updateScreenTitle", IDS_UPDATE_SCREEN_TITLE);
  builder->Add("updateScreenAccessibleTitle",
               IDS_UPDATE_SCREEN_ACCESSIBLE_TITLE);
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
  CallJS("enableUpdateCancel");
#endif
}

void UpdateScreenHandler::Hide() {
}

void UpdateScreenHandler::PrepareToShow() {
}

void UpdateScreenHandler::ShowManualRebootInfo() {
  CallJS("setUpdateMessage", l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED));
}

void UpdateScreenHandler::SetProgress(int progress) {
  CallJS("setUpdateProgress", progress);
}

void UpdateScreenHandler::ShowEstimatedTimeLeft(bool visible) {
  CallJS("showEstimatedTimeLeft", visible);
}

void UpdateScreenHandler::SetEstimatedTimeLeft(const base::TimeDelta& time) {
  CallJS("setEstimatedTimeLeft", time.InSecondsF());
}

void UpdateScreenHandler::ShowProgressMessage(bool visible) {
  CallJS("showProgressMessage", visible);
}

void UpdateScreenHandler::SetProgressMessage(ProgressMessage message) {
  int ids = 0;
  switch (message) {
    case PROGRESS_MESSAGE_UPDATE_AVAILABLE:
      ids = IDS_UPDATE_AVAILABLE;
      break;
    case PROGRESS_MESSAGE_INSTALLING_UPDATE:
      ids = IDS_INSTALLING_UPDATE;
      break;
    case PROGRESS_MESSAGE_VERIFYING:
      ids = IDS_UPDATE_VERIFYING;
      break;
    case PROGRESS_MESSAGE_FINALIZING:
      ids = IDS_UPDATE_FINALIZING;
      break;
    default:
      NOTREACHED();
      return;
  }

  CallJS("setProgressMessage", l10n_util::GetStringUTF16(ids));
}

void UpdateScreenHandler::ShowCurtain(bool visible) {
  CallJS("showUpdateCurtain", visible);
}

void UpdateScreenHandler::RegisterMessages() {
#if !defined(OFFICIAL_BUILD)
  AddCallback("cancelUpdate", &UpdateScreenHandler::HandleUpdateCancel);
#endif
}

void UpdateScreenHandler::OnConnectToNetworkRequested() {
  if (screen_)
    screen_->OnConnectToNetworkRequested();
}

#if !defined(OFFICIAL_BUILD)
void UpdateScreenHandler::HandleUpdateCancel() {
  screen_->CancelUpdate();
}
#endif

}  // namespace chromeos
