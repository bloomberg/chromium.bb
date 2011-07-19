// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/proxy_settings_dialog.h"

#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

// Hints for size of proxy settings dialog.
const int kProxySettingsDialogReasonableWidth = 750;
const int kProxySettingsDialogReasonableHeight = 550;
const float kProxySettingsDialogReasonableWidthRatio = 0.4f;
const float kProxySettingsDialogReasonableHeightRatio = 0.4f;

int CalculateSize(int screen_size, int min_comfortable, float desired_ratio) {
  int desired_size = static_cast<int>(desired_ratio * screen_size);
  desired_size = std::max(min_comfortable, desired_size);
  return std::min(screen_size, desired_size);
}

}  // namespace

namespace chromeos {

ProxySettingsDialog::ProxySettingsDialog(LoginHtmlDialog::Delegate* delegate,
                                         gfx::NativeWindow window)
    : LoginHtmlDialog(
          delegate,
          window,
          std::wstring(),
          GURL(chrome::kChromeUIProxySettingsURL),
          LoginHtmlDialog::STYLE_BUBBLE) {
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  SetDialogSize(CalculateSize(screen_bounds.width(),
                               kProxySettingsDialogReasonableWidth,
                               kProxySettingsDialogReasonableWidthRatio),
                CalculateSize(screen_bounds.height(),
                               kProxySettingsDialogReasonableHeight,
                               kProxySettingsDialogReasonableHeightRatio));
}

}  // namespace chromeos
