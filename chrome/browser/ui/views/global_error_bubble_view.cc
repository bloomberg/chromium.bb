// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/views/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "views/controls/image_view.h"

namespace {

enum {
  TAG_ACCEPT_BUTTON = 1,
  TAG_CANCEL_BUTTON,
};

const int kMaxBubbleViewWidth = 262;

// The horizontal padding between the title and the icon.
const int kTitleHorizontalPadding = 3;

// The vertical offset of the wrench bubble from the wrench menu button.
const int kWrenchBubblePointOffsetY = -6;

const int kLayoutBottomPadding = 2;

}  // namespace

GlobalErrorBubbleView::GlobalErrorBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::ArrowLocation location,
    const SkColor& color,
    Browser* browser,
    GlobalError* error)
    : BubbleDelegateView(anchor_view, location, color),
      browser_(browser),
      error_(error) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  int resource_id = error_->GetBubbleViewIconResourceID();
  scoped_ptr<views::ImageView> image_view(new views::ImageView());
  image_view->SetImage(rb.GetImageNamed(resource_id).ToSkBitmap());

  string16 title_string(error_->GetBubbleViewTitle());
  scoped_ptr<views::Label> title_label(new views::Label(title_string));
  title_label->SetMultiLine(true);
  title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label->SetFont(title_label->font().DeriveFont(1));

  string16 message_string(error_->GetBubbleViewMessage());
  views::Label* message_label = new views::Label(message_string);
  message_label->SetMultiLine(true);
  message_label->SizeToFit(kMaxBubbleViewWidth);
  message_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  string16 accept_string(error_->GetBubbleViewAcceptButtonLabel());
  scoped_ptr<views::TextButton> accept_button(
      new views::NativeTextButton(this, accept_string));
  accept_button->SetIsDefault(true);
  accept_button->set_tag(TAG_ACCEPT_BUTTON);

  string16 cancel_string(error_->GetBubbleViewCancelButtonLabel());
  scoped_ptr<views::TextButton> cancel_button;
  if (!cancel_string.empty()) {
    cancel_button.reset(new views::NativeTextButton(this, cancel_string));
    cancel_button->set_tag(TAG_CANCEL_BUTTON);
  }

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  layout->SetInsets(0, 0, kLayoutBottomPadding, 0);

  // Top row, icon and title.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING,
                0, views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, kTitleHorizontalPadding);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                1, views::GridLayout::USE_PREF, 0, 0);

  // Middle row, message label.
  cs = layout->AddColumnSet(1);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                1, views::GridLayout::USE_PREF, 0, 0);

  // Bottom row, accept and cancel button.
  cs = layout->AddColumnSet(2);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                0, views::GridLayout::USE_PREF, 0, 0);
  if (cancel_button.get()) {
    cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
    cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                  0, views::GridLayout::USE_PREF, 0, 0);
  }

  layout->StartRow(1, 0);
  layout->AddView(image_view.release());
  layout->AddView(title_label.release());
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(1, 1);
  layout->AddView(message_label);
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  layout->StartRow(0, 2);
  layout->AddView(accept_button.release());
  if (cancel_button.get())
    layout->AddView(cancel_button.release());

  // Adjust the message label size in case buttons are too long.
  message_label->SizeToFit(layout->GetPreferredSize(this).width());
}

GlobalErrorBubbleView::~GlobalErrorBubbleView() {
}

gfx::Point GlobalErrorBubbleView::GetAnchorPoint() {
  return (views::BubbleDelegateView::GetAnchorPoint().Add(
      gfx::Point(0, anchor_view() ? kWrenchBubblePointOffsetY : 0)));
}

void GlobalErrorBubbleView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender->tag() == TAG_ACCEPT_BUTTON)
    error_->BubbleViewAcceptButtonPressed();
  else if (sender->tag() == TAG_CANCEL_BUTTON)
    error_->BubbleViewCancelButtonPressed();
  else
    NOTREACHED();
  GetWidget()->Close();
}

void GlobalErrorBubbleView::WindowClosing() {
  error_->BubbleViewDidClose();
}

void GlobalError::ShowBubbleView(Browser* browser, GlobalError* error) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* wrench_button = browser_view->toolbar()->app_menu();
  GlobalErrorBubbleView* bubble_view =
      new GlobalErrorBubbleView(wrench_button,
                                views::BubbleBorder::TOP_RIGHT,
                                SK_ColorWHITE,
                                browser,
                                error);
  browser::CreateViewsBubble(bubble_view);
  bubble_view->StartFade(true);
}
