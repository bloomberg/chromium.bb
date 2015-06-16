// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/screens/update_model.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kJsScreenPath[] = "login.UpdateScreen";

}  // namespace

namespace chromeos {

UpdateScreenHandler::UpdateScreenHandler()
    : BaseScreenHandler(kJsScreenPath), model_(nullptr), show_on_init_(false) {
}

UpdateScreenHandler::~UpdateScreenHandler() {
  if (model_)
    model_->OnViewDestroyed(this);
}

void UpdateScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
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

void UpdateScreenHandler::PrepareToShow() {
}

void UpdateScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(OobeUI::kScreenOobeUpdate, NULL);
}

void UpdateScreenHandler::Hide() {
}

void UpdateScreenHandler::Bind(UpdateModel& model) {
  model_ = &model;
  BaseScreenHandler::SetBaseScreen(model_);
}

void UpdateScreenHandler::Unbind() {
  model_ = nullptr;
  BaseScreenHandler::SetBaseScreen(nullptr);
}

}  // namespace chromeos
