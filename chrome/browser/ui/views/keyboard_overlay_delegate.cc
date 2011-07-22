// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyboard_overlay_delegate.h"

#include <algorithm>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/screen.h"


static const int kBaseWidth = 1252;
static const int kBaseHeight = 516;
static const int kHorizontalMargin = 28;

KeyboardOverlayDelegate::KeyboardOverlayDelegate(
    const std::wstring& title)
    : title_(title),
      view_(NULL) {
}

KeyboardOverlayDelegate::~KeyboardOverlayDelegate() {
}

bool KeyboardOverlayDelegate::IsDialogModal() const {
  return true;
}

std::wstring KeyboardOverlayDelegate::GetDialogTitle() const {
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
  gfx::Rect rect = views::Screen::GetMonitorAreaNearestWindow(
      view_->native_view());
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

void KeyboardOverlayDelegate::OnCloseContents(TabContents* source,
                                              bool* out_close_dialog) {
}

bool KeyboardOverlayDelegate::ShouldShowDialogTitle() const {
  return false;
}

bool KeyboardOverlayDelegate::HandleContextMenu(
    const ContextMenuParams& params) {
  return true;
}
