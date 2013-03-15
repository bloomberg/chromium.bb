// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "content/public/browser/web_ui.h"
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

void UpdateScreenHandler::GetLocalizedStrings(
    DictionaryValue *localized_strings) {
  const string16 short_product_name =
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME);
  localized_strings->SetString("checkingForUpdatesMsg",
      l10n_util::GetStringFUTF16(IDS_CHECKING_FOR_UPDATE_MSG,
                                 short_product_name));
  localized_strings->SetString("updateScreenTitle",
      l10n_util::GetStringUTF16(IDS_UPDATE_SCREEN_TITLE));
  localized_strings->SetString("checkingForUpdates",
      l10n_util::GetStringUTF16(IDS_CHECKING_FOR_UPDATES));
  localized_strings->SetString("installingUpdateDesc",
      l10n_util::GetStringFUTF16(IDS_UPDATE_MSG, short_product_name));
  localized_strings->SetString("downloading",
      l10n_util::GetStringUTF16(IDS_DOWNLOADING));
  localized_strings->SetString("downloadingTimeLeftLong",
      l10n_util::GetStringUTF16(IDS_DOWNLOADING_TIME_LEFT_LONG));
  localized_strings->SetString("downloadingTimeLeftStatusOneHour",
      l10n_util::GetStringUTF16(IDS_DOWNLOADING_TIME_LEFT_STATUS_ONE_HOUR));
  localized_strings->SetString("downloadingTimeLeftStatusMinutes",
      l10n_util::GetStringUTF16(IDS_DOWNLOADING_TIME_LEFT_STATUS_MINUTES));
  localized_strings->SetString("downloadingTimeLeftSmall",
      l10n_util::GetStringUTF16(IDS_DOWNLOADING_TIME_LEFT_SMALL));
#if !defined(OFFICIAL_BUILD)
  localized_strings->SetString("cancelUpdateHint",
      l10n_util::GetStringUTF16(IDS_UPDATE_CANCEL));
  localized_strings->SetString("cancelledUpdateMessage",
      l10n_util::GetStringUTF16(IDS_UPDATE_CANCELLED));
#else
  localized_strings->SetString("cancelUpdateHint", "");
  localized_strings->SetString("cancelledUpdateMessage", "");
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
  web_ui()->CallJavascriptFunction("oobe.UpdateScreen.enableUpdateCancel");
#endif
}

void UpdateScreenHandler::Hide() {
}

void UpdateScreenHandler::PrepareToShow() {
}

void UpdateScreenHandler::ShowManualRebootInfo() {
  StringValue message(l10n_util::GetStringUTF16(IDS_UPDATE_COMPLETED));
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.setUpdateMessage", message);
}

void UpdateScreenHandler::SetProgress(int progress) {
  base::FundamentalValue progress_value(progress);
  web_ui()->CallJavascriptFunction("cr.ui.Oobe.setUpdateProgress",
                                   progress_value);
}

void UpdateScreenHandler::ShowEstimatedTimeLeft(bool visible) {
  base::FundamentalValue visible_value(visible);
  web_ui()->CallJavascriptFunction(
      "cr.ui.Oobe.showEstimatedTimeLeft", visible_value);
}

void UpdateScreenHandler::SetEstimatedTimeLeft(const base::TimeDelta& time) {
  base::FundamentalValue seconds_value(time.InSecondsF());
  web_ui()->CallJavascriptFunction(
      "cr.ui.Oobe.setEstimatedTimeLeft", seconds_value);
}

void UpdateScreenHandler::ShowProgressMessage(bool visible) {
  base::FundamentalValue visible_value(visible);
  web_ui()->CallJavascriptFunction(
      "cr.ui.Oobe.showProgressMessage", visible_value);
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
  if (progress_message.get()) {
    web_ui()->CallJavascriptFunction(
        "cr.ui.Oobe.setProgressMessage", *progress_message);
  }
}

void UpdateScreenHandler::ShowCurtain(bool visible) {
  base::FundamentalValue visible_value(visible);
  web_ui()->CallJavascriptFunction(
      "cr.ui.Oobe.showUpdateCurtain", visible_value);
}

void UpdateScreenHandler::RegisterMessages() {
#if !defined(OFFICIAL_BUILD)
  web_ui()->RegisterMessageCallback("cancelUpdate",
      base::Bind(&UpdateScreenHandler::HandleUpdateCancel,
                 base::Unretained(this)));
#endif
}

#if !defined(OFFICIAL_BUILD)
void UpdateScreenHandler::HandleUpdateCancel(const base::ListValue* args) {
  DCHECK(args && args->empty());
  screen_->CancelUpdate();
}
#endif

}  // namespace chromeos
