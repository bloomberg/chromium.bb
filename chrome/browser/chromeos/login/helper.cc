// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/google/google_util.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
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
  gfx::Rect bounds(gfx::Screen::GetPrimaryMonitor().bounds());
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

string16 GetCurrentNetworkName(NetworkLibrary* network_library) {
  if (!network_library)
    return string16();

  if (network_library->ethernet_connected()) {
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else if (network_library->wifi_connected()) {
    return UTF8ToUTF16(network_library->wifi_network()->name());
  } else if (network_library->cellular_connected()) {
    return UTF8ToUTF16(network_library->cellular_network()->name());
  } else if (network_library->wimax_connected()) {
    return UTF8ToUTF16(network_library->wimax_network()->name());
  } else if (network_library->ethernet_connecting()) {
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else if (network_library->wifi_connecting()) {
    return UTF8ToUTF16(network_library->wifi_network()->name());
  } else if (network_library->cellular_connecting()) {
    return UTF8ToUTF16(network_library->cellular_network()->name());
  } else if (network_library->wimax_connecting()) {
    return UTF8ToUTF16(network_library->wimax_network()->name());
  } else {
    return string16();
  }
}

}  // namespace chromeos
