// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_web_dialog.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/web_dialog_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/widget/widget.h"

using content::WebContents;
using content::WebUIMessageHandler;

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const double kDefaultWidthRatio = 0.6;
const double kDefaultHeightRatio = 0.6;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, public:

void LoginWebDialog::Delegate::OnDialogClosed() {
}

LoginWebDialog::LoginWebDialog(Delegate* delegate,
                               gfx::NativeWindow parent_window,
                               const string16& title,
                               const GURL& url,
                               Style style)
    : delegate_(delegate),
      parent_window_(parent_window),
      title_(title),
      url_(url),
      style_(style),
      bubble_frame_view_(NULL),
      is_open_(false) {
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  width_ = static_cast<int>(kDefaultWidthRatio * screen_bounds.width());
  height_ = static_cast<int>(kDefaultHeightRatio * screen_bounds.height());
}

LoginWebDialog::~LoginWebDialog() {
  delegate_ = NULL;
}

void LoginWebDialog::Show() {
  views::Widget::CreateWindowWithParent(
      new WebDialogView(ProfileManager::GetDefaultProfile(), NULL, this),
      parent_window_)->Show();
  is_open_ = true;
}

void LoginWebDialog::SetDialogSize(int width, int height) {
  DCHECK(width >= 0 && height >= 0);
  width_ = width;
  height_ = height;
}

void LoginWebDialog::SetDialogTitle(const string16& title) {
  title_ = title;
}

///////////////////////////////////////////////////////////////////////////////
// LoginWebDialog, protected:

ui::ModalType LoginWebDialog::GetDialogModalType() const {
  return ui::MODAL_TYPE_SYSTEM;
}

string16 LoginWebDialog::GetDialogTitle() const {
  return title_;
}

GURL LoginWebDialog::GetDialogContentURL() const {
  return url_;
}

void LoginWebDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void LoginWebDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(width_, height_);
}

std::string LoginWebDialog::GetDialogArgs() const {
  return std::string();
}

void LoginWebDialog::OnDialogClosed(const std::string& json_retval) {
  is_open_ = false;
  notification_registrar_.RemoveAll();
  if (delegate_)
    delegate_->OnDialogClosed();
}

void LoginWebDialog::OnCloseContents(WebContents* source,
                                     bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool LoginWebDialog::ShouldShowDialogTitle() const {
  return true;
}

bool LoginWebDialog::HandleContextMenu(
    const content::ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

void LoginWebDialog::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
  // TODO(saintlou): Do we need a throbber for Aura?
  NOTIMPLEMENTED();
}

}  // namespace chromeos
