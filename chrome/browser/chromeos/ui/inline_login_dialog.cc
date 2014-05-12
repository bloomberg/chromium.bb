// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui/inline_login_dialog.h"

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const int kGaiaViewHeight = 400;
const int kGaiaViewWidth = 360;

}  // namespace

namespace ui {

// static
void InlineLoginDialog::Show(content::BrowserContext* context) {
  InlineLoginDialog* dialog = new InlineLoginDialog();
  chrome::ShowWebDialog(NULL, context, dialog);
}

InlineLoginDialog::InlineLoginDialog() {
}

InlineLoginDialog::~InlineLoginDialog() {
}

ModalType InlineLoginDialog::GetDialogModalType() const {
  return MODAL_TYPE_SYSTEM;
}

base::string16 InlineLoginDialog::GetDialogTitle() const {
  return base::string16();
}

GURL InlineLoginDialog::GetDialogContentURL() const {
  return GURL(signin::GetPromoURL(signin::SOURCE_AVATAR_BUBBLE_ADD_ACCOUNT,
                                  false /* auto_close */,
                                  true /* is_constrained */));
}

void InlineLoginDialog::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {
}

void InlineLoginDialog::GetDialogSize(gfx::Size* size) const {
  *size = gfx::Size(kGaiaViewWidth, kGaiaViewHeight);
}

std::string InlineLoginDialog::GetDialogArgs() const {
  return "";
}

bool InlineLoginDialog::CanResizeDialog() const {
  return false;
}

void InlineLoginDialog::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void InlineLoginDialog::OnCloseContents(content::WebContents* source,
                                        bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool InlineLoginDialog::ShouldShowDialogTitle() const {
  return true;
}

}  // namespace ui
