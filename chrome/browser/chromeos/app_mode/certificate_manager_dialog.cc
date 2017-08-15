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
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/webview/web_dialog_view.h"

namespace {

// The width matches the Settings UI width.
const int kDefaultWidth = 640;
const int kDefaultHeight = 480;

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
                     GURL(chrome::kChromeUICertificateManagerDialogURL)) {}

CertificateManagerDialog::~CertificateManagerDialog() {}

base::string16 CertificateManagerDialog::GetDialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE);
}

void CertificateManagerDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDefaultWidth, kDefaultHeight);
}

}  // namespace chromeos
