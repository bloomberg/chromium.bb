// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include <stddef.h>

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_features.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/window/dialog_client_view.h"

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
#include "chrome/browser/ui/views/frame/browser_view.h"
#endif

namespace {

const int kMaxBubbleViewWidth = 362;

}  // namespace

// GlobalErrorBubbleViewBase ---------------------------------------------------

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
// static
GlobalErrorBubbleViewBase* GlobalErrorBubbleViewBase::ShowStandardBubbleView(
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* app_menu_button = browser_view->toolbar()->app_menu_button();
  GlobalErrorBubbleView* bubble_view =
      new GlobalErrorBubbleView(app_menu_button, gfx::Point(),
                                views::BubbleBorder::TOP_RIGHT, browser, error);
  views::BubbleDialogDelegateView::CreateBubble(bubble_view);
  bubble_view->GetWidget()->Show();
  return bubble_view;
}
#endif  // !OS_MACOSX || MAC_VIEWS_BROWSER

// GlobalErrorBubbleView -------------------------------------------------------

GlobalErrorBubbleView::GlobalErrorBubbleView(
    views::View* anchor_view,
    const gfx::Point& anchor_point,
    views::BubbleBorder::Arrow arrow,
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error)
    : BubbleDialogDelegateView(anchor_view, arrow),
      browser_(browser),
      error_(error) {
  if (!anchor_view)
    SetAnchorRect(gfx::Rect(anchor_point, gfx::Size()));
}

GlobalErrorBubbleView::~GlobalErrorBubbleView() {}

base::string16 GlobalErrorBubbleView::GetWindowTitle() const {
  if (!error_)
    return base::string16();
  return error_->GetBubbleViewTitle();
}

gfx::ImageSkia GlobalErrorBubbleView::GetWindowIcon() {
  gfx::Image image;
  if (error_) {
    image = error_->GetBubbleViewIcon();
    DCHECK(!image.IsEmpty());
  }
  return *image.ToImageSkia();
}

bool GlobalErrorBubbleView::ShouldShowWindowIcon() const {
  return true;
}

void GlobalErrorBubbleView::WindowClosing() {
  if (error_)
    error_->BubbleViewDidClose(browser_);
}

void GlobalErrorBubbleView::Init() {
  // |error_| is assumed to be valid, and stay valid, at least until Init()
  // returns.

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(
      GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));

  std::vector<base::string16> message_strings(error_->GetBubbleViewMessages());
  std::vector<views::Label*> message_labels;
  for (size_t i = 0; i < message_strings.size(); ++i) {
    views::Label* message_label = new views::Label(message_strings[i]);
    message_label->SetMultiLine(true);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_labels.push_back(message_label);
  }

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // First row, message labels.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                views::GridLayout::FIXED, kMaxBubbleViewWidth, 0);

  for (size_t i = 0; i < message_labels.size(); ++i) {
    layout->StartRow(1, 0);
    layout->AddView(message_labels[i]);
    if (i < message_labels.size() - 1)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }

  // These bubbles show at times where activation is sporadic (like at startup,
  // or a new window opening). Make sure the bubble doesn't disappear before the
  // user sees it, if the bubble needs to be acknowledged.
  set_close_on_deactivate(error_->ShouldCloseOnDeactivate());
}

void GlobalErrorBubbleView::UpdateButton(views::LabelButton* button,
                                         ui::DialogButton type) {
  if (error_) {
    // UpdateButton can result in calls back in to GlobalErrorBubbleView,
    // possibly accessing |error_|.
    BubbleDialogDelegateView::UpdateButton(button, type);
    if (type == ui::DIALOG_BUTTON_OK &&
        error_->ShouldAddElevationIconToAcceptButton()) {
      elevation_icon_setter_.reset(new ElevationIconSetter(
          button, base::Bind(&GlobalErrorBubbleView::SizeToContents,
                             base::Unretained(this))));
    }
  }
}

bool GlobalErrorBubbleView::ShouldShowCloseButton() const {
  return error_ && error_->ShouldShowCloseButton();
}

bool GlobalErrorBubbleView::ShouldDefaultButtonBeBlue() const {
  return true;
}

base::string16 GlobalErrorBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  if (!error_)
    return base::string16();
  return button == ui::DIALOG_BUTTON_OK
             ? error_->GetBubbleViewAcceptButtonLabel()
             : error_->GetBubbleViewCancelButtonLabel();
}

int GlobalErrorBubbleView::GetDialogButtons() const {
  if (!error_)
    return ui::DIALOG_BUTTON_NONE;
  return ui::DIALOG_BUTTON_OK |
         (error_->GetBubbleViewCancelButtonLabel().empty()
              ? 0
              : ui::DIALOG_BUTTON_CANCEL);
}

bool GlobalErrorBubbleView::Cancel() {
  if (error_)
    error_->BubbleViewCancelButtonPressed(browser_);
  return true;
}

bool GlobalErrorBubbleView::Accept() {
  if (error_)
    error_->BubbleViewAcceptButtonPressed(browser_);
  return true;
}

bool GlobalErrorBubbleView::Close() {
  // Don't fall through to either Cancel() or Accept().
  return true;
}

void GlobalErrorBubbleView::CloseBubbleView() {
  GetWidget()->Close();
}
