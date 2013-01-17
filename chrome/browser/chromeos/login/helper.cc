// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include "ash/shell.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/net/connectivity_state_helper.h"
#include "chrome/browser/google/google_util.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"

namespace chromeos {

namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

// Time in ms before smoothed throbber is shown.
const int kThrobberStartDelayMs = 500;

const SkColor kBackgroundCenterColor = SkColorSetRGB(41, 50, 67);
const SkColor kBackgroundEdgeColor = SK_ColorBLACK;

const char kAccountRecoveryHelpUrl[] =
    "https://www.google.com/support/accounts/bin/answer.py?answer=48598";

}  // namespace

views::SmoothedThrobber* CreateDefaultSmoothedThrobber() {
  views::SmoothedThrobber* throbber =
      new views::SmoothedThrobber(kThrobberFrameMs);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_SPINNER));
  throbber->set_start_delay_ms(kThrobberStartDelayMs);
  return throbber;
}

views::Throbber* CreateDefaultThrobber() {
  views::Throbber* throbber = new views::Throbber(kThrobberFrameMs, false);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(IDR_SPINNER));
  return throbber;
}

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds(ash::Shell::GetScreen()->GetPrimaryDisplay().bounds());
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

void CorrectLabelFontSize(views::Label* label) {
  if (label)
    label->SetFont(label->font().DeriveFont(kFontSizeCorrectionDelta));
}

void CorrectTextfieldFontSize(views::Textfield* textfield) {
  if (textfield)
    textfield->SetFont(textfield->font().DeriveFont(kFontSizeCorrectionDelta));
}

GURL GetAccountRecoveryHelpUrl() {
  return google_util::AppendGoogleLocaleParam(GURL(kAccountRecoveryHelpUrl));
}

string16 GetCurrentNetworkName() {
  ConnectivityStateHelper* csh =
      ConnectivityStateHelper::Get();

  if (csh->IsConnectedType(flimflam::kTypeEthernet)) {
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else if (csh->IsConnectedType(flimflam::kTypeWifi)) {
    return UTF8ToUTF16(csh->NetworkNameForType(flimflam::kTypeWifi));
  } else if (csh->IsConnectedType(flimflam::kTypeCellular)) {
    return UTF8ToUTF16(csh->NetworkNameForType(flimflam::kTypeCellular));
  } else if (csh->IsConnectedType(flimflam::kTypeWimax)) {
    return UTF8ToUTF16(csh->NetworkNameForType(flimflam::kTypeWimax));
  } else if (csh->IsConnectingType(flimflam::kTypeEthernet)) {
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else if (csh->IsConnectingType(flimflam::kTypeWifi)) {
    return UTF8ToUTF16(csh->NetworkNameForType(flimflam::kTypeWifi));
  } else if (csh->IsConnectingType(flimflam::kTypeCellular)) {
    return UTF8ToUTF16(csh->NetworkNameForType(flimflam::kTypeCellular));
  } else if (csh->IsConnectingType(flimflam::kTypeWimax)) {
    return UTF8ToUTF16(csh->NetworkNameForType(flimflam::kTypeWimax));
  } else {
    return string16();
  }
}

int GetCurrentUserImageSize() {
  // The biggest size that the profile picture is displayed at is currently
  // 220px, used for the big preview on OOBE and Change Picture options page.
  static const int kBaseUserImageSize = 220;
  float scale_factor = gfx::Display::GetForcedDeviceScaleFactor();
  if (scale_factor > 1.0f)
    return static_cast<int>(scale_factor * kBaseUserImageSize);
  return kBaseUserImageSize *
      ui::GetScaleFactorScale(ui::GetMaxScaleFactor());
}

}  // namespace chromeos
