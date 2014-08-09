// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/certificate_manager_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/webview/web_dialog_view.h"

namespace {

// Hints for size of certificate manager dialog.
const int kCertificateManagerDialogReasonableWidth = 712;
const int kCertificateManagerDialogReasonableHeight = 412;
const float kCertificateManagerDialogReasonableWidthRatio = 0.4f;
const float kCertificateManagerDialogReasonableHeightRatio = 0.4f;

int CalculateSize(int screen_size, int min_comfortable, float desired_ratio) {
  int desired_size = static_cast<int>(desired_ratio * screen_size);
  desired_size = std::max(min_comfortable, desired_size);
  return std::min(screen_size, desired_size);
}

}  // namespace

namespace chromeos {

CertificateManagerDialog::CertificateManagerDialog(
    Profile* profile,
    LoginWebDialog::Delegate* delegate,
    gfx::NativeWindow window)
    : LoginWebDialog(profile,
                     delegate,
                     window,
                     base::string16(),
                     GURL(chrome::kChromeUICertificateManagerDialogURL),
                     LoginWebDialog::STYLE_BUBBLE) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  SetDialogSize(CalculateSize(screen_bounds.width(),
                              kCertificateManagerDialogReasonableWidth,
                              kCertificateManagerDialogReasonableWidthRatio),
                CalculateSize(screen_bounds.height(),
                              kCertificateManagerDialogReasonableHeight,
                              kCertificateManagerDialogReasonableHeightRatio));

  SetDialogTitle(l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE));
}

CertificateManagerDialog::~CertificateManagerDialog() {
}


}  // namespace chromeos
