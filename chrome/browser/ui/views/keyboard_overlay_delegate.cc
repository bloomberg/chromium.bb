// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyboard_overlay_delegate.h"

#include <algorithm>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/web_dialog_view.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace {

const int kBaseWidth = 1252;
const int kBaseHeight = 516;
const int kHorizontalMargin = 28;

}  // namespace

KeyboardOverlayDelegate::KeyboardOverlayDelegate(const string16& title)
    : title_(title),
      view_(NULL) {
}

KeyboardOverlayDelegate::~KeyboardOverlayDelegate() {
}

ui::ModalType KeyboardOverlayDelegate::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 KeyboardOverlayDelegate::GetDialogTitle() const {
  return title_;
}

GURL KeyboardOverlayDelegate::GetDialogContentURL() const {
  std::string url_string(chrome::kChromeUIKeyboardOverlayURL);
  return GURL(url_string);
}

void KeyboardOverlayDelegate::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void KeyboardOverlayDelegate::GetDialogSize(
    gfx::Size* size) const {
  using std::min;
  DCHECK(view_);
  gfx::Rect rect = gfx::Screen::GetMonitorNearestWindow(
      view_->GetWidget()->GetNativeView()).bounds();
  const int width = min(kBaseWidth, rect.width() - kHorizontalMargin);
  const int height = width * kBaseHeight / kBaseWidth;
  size->SetSize(width, height);
}

std::string KeyboardOverlayDelegate::GetDialogArgs() const {
  return "[]";
}

void KeyboardOverlayDelegate::OnDialogClosed(
    const std::string& json_retval) {
  delete this;
  return;
}

void KeyboardOverlayDelegate::OnCloseContents(WebContents* source,
                                              bool* out_close_dialog) {
}

bool KeyboardOverlayDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool KeyboardOverlayDelegate::HandleContextMenu(
    const content::ContextMenuParams& params) {
  return true;
}
