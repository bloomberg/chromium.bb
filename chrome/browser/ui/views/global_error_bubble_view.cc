// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/views/elevation_icon_setter.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/wrench_toolbar_button.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

enum {
  TAG_ACCEPT_BUTTON = 1,
  TAG_CANCEL_BUTTON,
};

const int kMaxBubbleViewWidth = 262;

// The vertical inset of the wrench bubble anchor from the wrench menu button.
const int kAnchorVerticalInset = 5;

const int kBubblePadding = 19;

// Spacing between bubble text and buttons.
const int kLabelToButtonVerticalSpacing = 14;

}  // namespace

// GlobalErrorBubbleViewBase ---------------------------------------------------

// static
GlobalErrorBubbleViewBase* GlobalErrorBubbleViewBase::ShowStandardBubbleView(
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* wrench_button = browser_view->toolbar()->app_menu();
  GlobalErrorBubbleView* bubble_view =
      new GlobalErrorBubbleView(wrench_button,
                                views::BubbleBorder::TOP_RIGHT,
                                browser,
                                error);
  views::BubbleDelegateView::CreateBubble(bubble_view);
  bubble_view->GetWidget()->Show();
  return bubble_view;
}

// GlobalErrorBubbleView -------------------------------------------------------

GlobalErrorBubbleView::GlobalErrorBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error)
    : BubbleDelegateView(anchor_view, arrow),
      browser_(browser),
      error_(error) {
  // Set content margins to left-align the bubble text with the title.
  // BubbleFrameView adds enough padding below title, no top padding needed.
  set_margins(gfx::Insets(0, kBubblePadding, kBubblePadding, kBubblePadding));

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(
      gfx::Insets(kAnchorVerticalInset, 0, kAnchorVerticalInset, 0));

  std::vector<base::string16> message_strings(error_->GetBubbleViewMessages());
  std::vector<views::Label*> message_labels;
  for (size_t i = 0; i < message_strings.size(); ++i) {
    views::Label* message_label = new views::Label(message_strings[i]);
    message_label->SetMultiLine(true);
    message_label->SizeToFit(kMaxBubbleViewWidth);
    message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message_labels.push_back(message_label);
  }

  base::string16 accept_string(error_->GetBubbleViewAcceptButtonLabel());
  scoped_ptr<views::LabelButton> accept_button(
      new views::LabelButton(this, accept_string));
  accept_button->SetStyle(views::Button::STYLE_BUTTON);
  accept_button->SetIsDefault(true);
  accept_button->set_tag(TAG_ACCEPT_BUTTON);
  if (error_->ShouldAddElevationIconToAcceptButton())
    elevation_icon_setter_.reset(
        new ElevationIconSetter(
            accept_button.get(),
            base::Bind(&GlobalErrorBubbleView::SizeToContents,
                       base::Unretained(this))));

  base::string16 cancel_string(error_->GetBubbleViewCancelButtonLabel());
  scoped_ptr<views::LabelButton> cancel_button;
  if (!cancel_string.empty()) {
    cancel_button.reset(new views::LabelButton(this, cancel_string));
    cancel_button->SetStyle(views::Button::STYLE_BUTTON);
    cancel_button->set_tag(TAG_CANCEL_BUTTON);
  }

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // First row, message labels.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                1, views::GridLayout::USE_PREF, 0, 0);

  // Second row, accept and cancel button.
  cs = layout->AddColumnSet(1);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                0, views::GridLayout::USE_PREF, 0, 0);
  if (cancel_button.get()) {
    cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
    cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                  0, views::GridLayout::USE_PREF, 0, 0);
  }

  for (size_t i = 0; i < message_labels.size(); ++i) {
    layout->StartRow(1, 0);
    layout->AddView(message_labels[i]);
    if (i < message_labels.size() - 1)
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  layout->AddPaddingRow(0, kLabelToButtonVerticalSpacing);

  layout->StartRow(0, 1);
  layout->AddView(accept_button.release());
  if (cancel_button.get())
    layout->AddView(cancel_button.release());

  // Adjust the message label size in case buttons are too long.
  for (size_t i = 0; i < message_labels.size(); ++i)
    message_labels[i]->SizeToFit(layout->GetPreferredSize(this).width());

  // These bubbles show at times where activation is sporadic (like at startup,
  // or a new window opening). Make sure the bubble doesn't disappear before the
  // user sees it, if the bubble needs to be acknowledged.
  set_close_on_deactivate(error_->ShouldCloseOnDeactivate());
}

GlobalErrorBubbleView::~GlobalErrorBubbleView() {
  // |elevation_icon_setter_| references |accept_button_|, so make sure it is
  // destroyed before |accept_button_|.
  elevation_icon_setter_.reset();
}

void GlobalErrorBubbleView::ButtonPressed(views::Button* sender,
                                          const ui::Event& event) {
  if (error_) {
    if (sender->tag() == TAG_ACCEPT_BUTTON)
      error_->BubbleViewAcceptButtonPressed(browser_);
    else if (sender->tag() == TAG_CANCEL_BUTTON)
      error_->BubbleViewCancelButtonPressed(browser_);
    else
      NOTREACHED();
  }
  GetWidget()->Close();
}

base::string16 GlobalErrorBubbleView::GetWindowTitle() const {
  return error_->GetBubbleViewTitle();
}

gfx::ImageSkia GlobalErrorBubbleView::GetWindowIcon() {
  gfx::Image image = error_->GetBubbleViewIcon();
  DCHECK(!image.IsEmpty());
  return *image.ToImageSkia();
}

bool GlobalErrorBubbleView::ShouldShowWindowIcon() const {
  return true;
}

void GlobalErrorBubbleView::WindowClosing() {
  if (error_)
    error_->BubbleViewDidClose(browser_);
}

bool GlobalErrorBubbleView::ShouldShowCloseButton() const {
  return error_ && error_->ShouldShowCloseButton();
}

void GlobalErrorBubbleView::CloseBubbleView() {
  GetWidget()->Close();
}
