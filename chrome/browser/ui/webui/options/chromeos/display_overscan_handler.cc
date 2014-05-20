// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/display_overscan_handler.h"

#include <string>

#include "ash/display/display_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/overscan_calibrator.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace chromeos {
namespace options {
namespace {

// The value for the orientation of overscan operations.
const char kOrientationHorizontal[] = "horizontal";
const char kOrientationVertical[] = "vertical";

}

DisplayOverscanHandler::DisplayOverscanHandler() {
  ash::Shell::GetScreen()->AddObserver(this);
}

DisplayOverscanHandler::~DisplayOverscanHandler() {
  ash::Shell::GetScreen()->RemoveObserver(this);
}

void DisplayOverscanHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  RegisterTitle(localized_strings, "displayOverscanPage",
                IDS_OPTIONS_SETTINGS_DISPLAY_OVERSCAN_TAB_TITLE);
  localized_strings->SetString("shrinkAndExpand", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OVERSCAN_SHRINK_EXPAND));
  localized_strings->SetString("move", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OVERSCAN_MOVE));
  localized_strings->SetString("overscanReset", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OVERSCAN_RESET_BUTTON_LABEL));
  localized_strings->SetString("overscanOK", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OVERSCAN_OK_BUTTON_LABEL));
  localized_strings->SetString("overscanCancel", l10n_util::GetStringUTF16(
      IDS_OPTIONS_SETTINGS_DISPLAY_OVERSCAN_CANCEL_BUTTON_LABEL));
}

void DisplayOverscanHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "start",
      base::Bind(&DisplayOverscanHandler::HandleStart,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "commit",
      base::Bind(&DisplayOverscanHandler::HandleCommit,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "reset",
      base::Bind(&DisplayOverscanHandler::HandleReset,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel",
      base::Bind(&DisplayOverscanHandler::HandleCancel,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "move",
      base::Bind(&DisplayOverscanHandler::HandleMove,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resize",
      base::Bind(&DisplayOverscanHandler::HandleResize,
                 base::Unretained(this)));
}

void DisplayOverscanHandler::OnDisplayAdded(const gfx::Display& new_display) {
  web_ui()->CallJavascriptFunction(
      "options.DisplayOverscan.onOverscanCanceled");
}

void DisplayOverscanHandler::OnDisplayRemoved(const gfx::Display& old_display) {
  web_ui()->CallJavascriptFunction(
      "options.DisplayOverscan.onOverscanCanceled");
}

void DisplayOverscanHandler::OnDisplayMetricsChanged(const gfx::Display&,
                                                     uint32_t) {
}

void DisplayOverscanHandler::HandleStart(const base::ListValue* args) {
  int64 display_id = gfx::Display::kInvalidDisplayID;
  std::string id_value;
  if (!args->GetString(0, &id_value)) {
    LOG(ERROR) << "Can't find ID";
    return;
  }

  if (!base::StringToInt64(id_value, &display_id) ||
      display_id == gfx::Display::kInvalidDisplayID) {
    LOG(ERROR) << "Invalid parameter: " << id_value;
    return;
  }

  const gfx::Display& display = ash::ScreenUtil::GetDisplayForId(display_id);
  DCHECK(display.is_valid());
  if (!display.is_valid())
    return;

  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  overscan_calibrator_.reset(new OverscanCalibrator(
      display,
      display_controller->GetOverscanInsets(display_id)));
}

void DisplayOverscanHandler::HandleCommit(const base::ListValue* unused_args) {
  if (overscan_calibrator_.get()) {
    overscan_calibrator_->Commit();
    overscan_calibrator_.reset();
  }
}

void DisplayOverscanHandler::HandleReset(const base::ListValue* unused_args) {
  if (overscan_calibrator_.get())
    overscan_calibrator_->Reset();
}

void DisplayOverscanHandler::HandleCancel(const base::ListValue* unused_args) {
  overscan_calibrator_.reset();
}

void DisplayOverscanHandler::HandleMove(const base::ListValue* args) {
  std::string orientation;
  double length;
  if (!args->GetString(0, &orientation)) {
    LOG(ERROR) << "The first argument must be orientation";
    return;
  }
  if (!args->GetDouble(1, &length)) {
    LOG(ERROR) << "The second argument must be a numeric";
    return;
  }

  if (!overscan_calibrator_.get())
    return;

  gfx::Insets insets = overscan_calibrator_->insets();
  if (orientation == kOrientationHorizontal) {
    insets.Set(insets.top(), insets.left() + length,
               insets.bottom(), insets.right() - length);
  } else if (orientation == kOrientationVertical) {
    insets.Set(insets.top() + length, insets.left(),
               insets.bottom() - length, insets.right());
  } else {
    LOG(ERROR) << "The orientation must be '" << kOrientationHorizontal
               << "' or '" << kOrientationVertical << "': "
               << orientation;
    return;
  }
  overscan_calibrator_->UpdateInsets(insets);
}

void DisplayOverscanHandler::HandleResize(const base::ListValue* args) {
  std::string orientation;
  double length;
  if (!args->GetString(0, &orientation)) {
    LOG(ERROR) << "The first argument must be orientation";
    return;
  }
  if (!args->GetDouble(1, &length)) {
    LOG(ERROR) << "The second argument must be a numeric";
    return;
  }

  if (!overscan_calibrator_.get())
    return;

  gfx::Insets insets = overscan_calibrator_->insets();
  if (orientation == kOrientationHorizontal) {
    insets.Set(insets.top(), insets.left() + length,
               insets.bottom(), insets.right() + length);
  } else if (orientation == kOrientationVertical) {
    insets.Set(insets.top() + length, insets.left(),
               insets.bottom() + length, insets.right());
  } else {
    LOG(ERROR) << "The orientation must be '" << kOrientationHorizontal
               << "' or '" << kOrientationVertical << "': "
               << orientation;
    return;
  }
  overscan_calibrator_->UpdateInsets(insets);
}

}  // namespace options
}  // namespace chromeos
