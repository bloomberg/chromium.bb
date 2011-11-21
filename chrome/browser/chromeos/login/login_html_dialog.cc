// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_html_dialog.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/frame/bubble_frame_view.h"
#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/widget/widget.h"

namespace chromeos {

namespace {

// Default width/height ratio of screen size.
const double kDefaultWidthRatio = 0.6;
const double kDefaultHeightRatio = 0.6;

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// LoginHtmlDialog, public:

LoginHtmlDialog::LoginHtmlDialog(Delegate* delegate,
                                 gfx::NativeWindow parent_window,
                                 const std::wstring& title,
                                 const GURL& url,
                                 Style style)
    : delegate_(delegate),
      parent_window_(parent_window),
      title_(WideToUTF16Hack(title)),
      url_(url),
      style_(style),
      bubble_frame_view_(NULL),
      is_open_(false) {
  gfx::Rect screen_bounds(chromeos::CalculateScreenBounds(gfx::Size()));
  width_ = static_cast<int>(kDefaultWidthRatio * screen_bounds.width());
  height_ = static_cast<int>(kDefaultHeightRatio * screen_bounds.height());
}

LoginHtmlDialog::~LoginHtmlDialog() {
  delegate_ = NULL;
}

void LoginHtmlDialog::Show() {
  HtmlDialogView* html_view =
      new HtmlDialogView(ProfileManager::GetDefaultProfile(), this);
#if defined(USE_AURA)
  // TODO(saintlou): Until the new Bubble have been landed.
  views::Widget::CreateWindowWithParent(html_view, parent_window_);
  html_view->InitDialog();
#else
  if (style_ & STYLE_BUBBLE) {
    views::Widget* bubble_window = BubbleWindow::Create(parent_window_,
        static_cast<DialogStyle>(STYLE_XBAR | STYLE_THROBBER),
        html_view);
    bubble_frame_view_ = static_cast<BubbleFrameView*>(
        bubble_window->non_client_view()->frame_view());
  } else {
    views::Widget::CreateWindowWithParent(html_view, parent_window_);
  }
  html_view->InitDialog();
  if (bubble_frame_view_) {
    bubble_frame_view_->StartThrobber();
    notification_registrar_.Add(
        this, content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
        content::Source<TabContents>(
            html_view->dom_contents()->tab_contents()));
  }
#endif
  html_view->GetWidget()->Show();
  is_open_ = true;
}

void LoginHtmlDialog::SetDialogSize(int width, int height) {
  DCHECK(width >= 0 && height >= 0);
  width_ = width;
  height_ = height;
}

///////////////////////////////////////////////////////////////////////////////
// LoginHtmlDialog, protected:

bool LoginHtmlDialog::IsDialogModal() const {
  return true;
}

string16 LoginHtmlDialog::GetDialogTitle() const {
  return title_;
}

GURL LoginHtmlDialog::GetDialogContentURL() const {
  return url_;
}

void LoginHtmlDialog::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
}

void LoginHtmlDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(width_, height_);
}

std::string LoginHtmlDialog::GetDialogArgs() const {
  return std::string();
}

void LoginHtmlDialog::OnDialogClosed(const std::string& json_retval) {
  is_open_ = false;
  notification_registrar_.RemoveAll();
  if (delegate_)
    delegate_->OnDialogClosed();
}

void LoginHtmlDialog::OnCloseContents(TabContents* source,
                                      bool* out_close_dialog) {
  if (out_close_dialog)
    *out_close_dialog = true;
}

bool LoginHtmlDialog::ShouldShowDialogTitle() const {
  return true;
}

bool LoginHtmlDialog::HandleContextMenu(const ContextMenuParams& params) {
  // Disable context menu.
  return true;
}

void LoginHtmlDialog::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME);
#if defined(USE_AURA)
  // TODO(saintlou): Do we need a throbber for Aura?
  NOTIMPLEMENTED();
#else
  if (bubble_frame_view_)
    bubble_frame_view_->StopThrobber();
#endif
}

}  // namespace chromeos
