// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/global_error_bubble_view.h"

#include "base/logging.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

namespace {

enum {
  TAG_ACCEPT_BUTTON = 1,
  TAG_CANCEL_BUTTON,
};

const int kBubbleViewWidth = 262;

// The horizontal padding between the title and the icon.
const int kTitleHorizontalPadding = 3;

// The vertical offset of the wrench bubble from the wrench menu button.
const int kWrenchBubblePointOffsetY = 6;

}  // namespace

GlobalErrorBubbleView::GlobalErrorBubbleView(Browser* browser,
                                             GlobalError* error)
    : browser_(browser),
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
  message_label_ = new views::Label(message_string);
  message_label_->SetMultiLine(true);
  message_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

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
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING,
                0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(1, 0);
  layout->AddView(image_view.release());
  layout->AddView(title_label.release());
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  layout->StartRow(1, 1);
  layout->AddView(message_label_);
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  layout->StartRow(0, 2);
  layout->AddView(accept_button.release());
  if (cancel_button.get())
    layout->AddView(cancel_button.release());
  else
    layout->SkipColumns(1);
}

GlobalErrorBubbleView::~GlobalErrorBubbleView() {
}

gfx::Size GlobalErrorBubbleView::GetPreferredSize() {
  views::GridLayout* layout =
      static_cast<views::GridLayout*>(GetLayoutManager());
  // Buttons row may require bigger width than |kBubbleViewWidth|. To support
  // this case, we first set the message label to fit our preferred width.
  // Then, get the desired width from GridLayout (it may be bigger than
  // |kBubbleViewWidth| if button text is long enough). This width is used as
  // the final width for our view, so message label's preferred width is reset
  // back to 0.
  message_label_->SizeToFit(kBubbleViewWidth);
  int width = std::max(layout->GetPreferredSize(this).width(),
                       kBubbleViewWidth);
  message_label_->SizeToFit(0);
  int height = layout->GetPreferredHeightForWidth(this, width);
  return gfx::Size(width, height);
}

void GlobalErrorBubbleView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  DCHECK(bubble_);
  if (sender->tag() == TAG_ACCEPT_BUTTON)
    error_->BubbleViewAcceptButtonPressed();
  else if (sender->tag() == TAG_CANCEL_BUTTON)
    error_->BubbleViewCancelButtonPressed();
  else
    NOTREACHED();
  bubble_->Close();
}

void GlobalErrorBubbleView::BubbleClosing(Bubble* bubble,
                                          bool closed_by_escape) {
  error_->BubbleViewDidClose();
}

bool GlobalErrorBubbleView::CloseOnEscape() {
  return true;
}

bool GlobalErrorBubbleView::FadeInOnShow() {
  // TODO(sail): Enabling fade causes the window to be disabled for some
  // reason. Until this is fixed we need to disable fade.
  return false;
}

void GlobalError::ShowBubbleView(Browser* browser, GlobalError* error) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* wrench_button = browser_view->toolbar()->app_menu();

  gfx::Point origin;
  views::View::ConvertPointToScreen(wrench_button, &origin);
  gfx::Rect bounds(origin.x(), origin.y(), wrench_button->width(),
                   wrench_button->height());
  bounds.Inset(0, kWrenchBubblePointOffsetY);

  GlobalErrorBubbleView* bubble_view =
      new GlobalErrorBubbleView(browser, error);
  // Bubble::Show() takes ownership of the view.
  Bubble* bubble = Bubble::Show(
      browser_view->GetWidget(), bounds, views::BubbleBorder::TOP_RIGHT,
      views::BubbleBorder::ALIGN_ARROW_TO_MID_ANCHOR,  bubble_view,
      bubble_view);
  bubble_view->set_bubble(bubble);
}
