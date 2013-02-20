// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include "ash/shell.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/net/connectivity_state_helper.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/textfield/textfield.h"

namespace chromeos {

namespace {

// Font size correction in pixels for login/oobe controls.
const int kFontSizeCorrectionDelta = 2;

}  // namespace

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds(ash::Shell::GetScreen()->GetPrimaryDisplay().bounds());
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

void CorrectTextfieldFontSize(views::Textfield* textfield) {
  if (textfield)
    textfield->SetFont(textfield->font().DeriveFont(kFontSizeCorrectionDelta));
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
