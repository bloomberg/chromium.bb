// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/inline_login_handler_dialog_chromeos.h"

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/gurl.h"

namespace chromeos {

namespace {

InlineLoginHandlerDialogChromeOS* dialog = nullptr;
constexpr int kSigninDialogWidth = 768;
constexpr int kSigninDialogHeight = 640;

}  // namespace

// static
void InlineLoginHandlerDialogChromeOS::Show(const std::string& email) {
  if (dialog) {
    dialog->dialog_window()->Focus();
    return;
  }

  GURL url(chrome::kChromeUIChromeSigninURL);
  if (!email.empty()) {
    url = net::AppendQueryParameter(url, "email", email);
    url = net::AppendQueryParameter(url, "readOnlyEmail", "true");
  }

  // Will be deleted by |SystemWebDialogDelegate::OnDialogClosed|.
  dialog = new InlineLoginHandlerDialogChromeOS(url);
  dialog->ShowSystemDialog();
}

InlineLoginHandlerDialogChromeOS::InlineLoginHandlerDialogChromeOS(
    const GURL& url)
    : SystemWebDialogDelegate(url, base::string16() /* title */) {}

InlineLoginHandlerDialogChromeOS::~InlineLoginHandlerDialogChromeOS() {
  DCHECK_EQ(this, dialog);
  dialog = nullptr;
}

void InlineLoginHandlerDialogChromeOS::GetDialogSize(gfx::Size* size) const {
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(dialog_window());
  size->SetSize(std::min(kSigninDialogWidth, display.work_area().width()),
                std::min(kSigninDialogHeight, display.work_area().height()));
}

std::string InlineLoginHandlerDialogChromeOS::GetDialogArgs() const {
  return std::string();
}

bool InlineLoginHandlerDialogChromeOS::ShouldShowDialogTitle() const {
  return false;
}

}  // namespace chromeos
