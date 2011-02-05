// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_html_dialog.h"

#include "chrome/browser/chromeos/frame/bubble_frame_view.h"
#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/html_dialog_view.h"
#include "chrome/common/notification_service.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/window/window.h"

namespace chromeos {

namespace {
// Default width/height ratio of screen size.
const double kDefaultWidthRatio = 0.6;
const double kDefaultHeightRatio = 0.6;

// Custom HtmlDialogView with disabled context menu.
class HtmlDialogWithoutContextMenuView : public HtmlDialogView {
 public:
  HtmlDialogWithoutContextMenuView(Profile* profile,
                                   HtmlDialogUIDelegate* delegate)
      : HtmlDialogView(profile, delegate) {}
  virtual ~HtmlDialogWithoutContextMenuView() {}

  // TabContentsDelegate implementation.
  bool HandleContextMenu(const ContextMenuParams& params) {
    // Disable context menu.
    return true;
  }
};

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
      title_(title),
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
  HtmlDialogWithoutContextMenuView* html_view =
      new HtmlDialogWithoutContextMenuView(ProfileManager::GetDefaultProfile(),
                                           this);
  if (style_ & STYLE_BUBBLE) {
    views::Window* bubble_window = BubbleWindow::Create(
        parent_window_, gfx::Rect(),
        static_cast<BubbleWindow::Style>(
            BubbleWindow::STYLE_XBAR | BubbleWindow::STYLE_THROBBER),
        html_view);
    bubble_frame_view_ = static_cast<BubbleFrameView*>(
        bubble_window->GetNonClientView()->frame_view());
  } else {
    views::Window::CreateChromeWindow(parent_window_, gfx::Rect(), html_view);
  }
  if (bubble_frame_view_) {
    bubble_frame_view_->StartThrobber();
    notification_registrar_.Add(this,
                                NotificationType::LOAD_COMPLETED_MAIN_FRAME,
                                NotificationService::AllSources());
  }
  html_view->InitDialog();
  html_view->window()->Show();
  is_open_ = true;
}

void LoginHtmlDialog::SetDialogSize(int width, int height) {
  DCHECK(width >= 0 && height >= 0);
  width_ = width;
  height_ = height;
}

void LoginHtmlDialog::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  DCHECK(type.value == NotificationType::LOAD_COMPLETED_MAIN_FRAME);
  if (bubble_frame_view_)
    bubble_frame_view_->StopThrobber();
}

///////////////////////////////////////////////////////////////////////////////
// LoginHtmlDialog, protected:

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

void LoginHtmlDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(width_, height_);
}

}  // namespace chromeos
