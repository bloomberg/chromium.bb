// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/window.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/events/event.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

// Minimum width for the fields - they will push out the size of the bubble if
// necessary. This should be big enough so that the field pushes the right side
// of the bubble far enough so that the edit button's left edge is to the right
// of the field's left edge.
const int kMinimumFieldSize = 240;

// BookmarkBubbleView ---------------------------------------------------------

// static
OneClickSigninBubbleView* OneClickSigninBubbleView::bubble_view_ = NULL;

// static
void OneClickSigninBubbleView::ShowBubble(
    views::View* anchor_view,
    const base::Closure& learn_more_callback,
    const base::Closure& advanced_callback) {
  if (IsShowing())
    return;

  bubble_view_ =
      new OneClickSigninBubbleView(anchor_view, learn_more_callback,
                                   advanced_callback);
  views::BubbleDelegateView::CreateBubble(bubble_view_);
  bubble_view_->Show();
}

// static
bool OneClickSigninBubbleView::IsShowing() {
  return bubble_view_ != NULL;
}

// static
void OneClickSigninBubbleView::Hide() {
  if (IsShowing())
    bubble_view_->GetWidget()->Close();
}

OneClickSigninBubbleView::OneClickSigninBubbleView(
    views::View* anchor_view,
    const base::Closure& learn_more_callback,
    const base::Closure& advanced_callback)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      learn_more_link_(NULL),
      advanced_link_(NULL),
      close_button_(NULL),
      learn_more_callback_(learn_more_callback),
      advanced_callback_(advanced_callback),
      message_loop_for_testing_(NULL) {
  DCHECK(!learn_more_callback_.is_null());
  DCHECK(!advanced_callback_.is_null());
}

OneClickSigninBubbleView::~OneClickSigninBubbleView() {
}

void OneClickSigninBubbleView::AnimationEnded(const ui::Animation* animation) {
  views::BubbleDelegateView::AnimationEnded(animation);
  if (message_loop_for_testing_)
    message_loop_for_testing_->Quit();
}

void OneClickSigninBubbleView::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);
  set_border(views::Border::CreateEmptyBorder(8, 8, 8, 8));

  enum {
    kColumnSetFillAlign,
    kColumnSetControls
  };

  // Column set for descriptive text and link.
  views::ColumnSet* cs = layout->AddColumnSet(kColumnSetFillAlign);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, kMinimumFieldSize);

  cs = layout->AddColumnSet(kColumnSetControls);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, 0);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);

  // Add main text description.
  views::Label* label = new views::Label(
      l10n_util::GetStringFUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_MESSAGE,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SizeToFit(kMinimumFieldSize);

  layout->StartRow(0, kColumnSetFillAlign);
  layout->AddView(label);

  // Add link for user to learn more about sync.
  learn_more_link_= new views::Link(
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_LEARN_MORE));
  learn_more_link_->set_listener(this);
  learn_more_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  layout->StartRow(0, kColumnSetFillAlign);
  layout->AddView(learn_more_link_);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  // Add link for user to do advanced config of sync.
  advanced_link_= new views::Link(
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  advanced_link_->set_listener(this);
  advanced_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Add controls at the bottom.
  close_button_ = new views::NativeTextButton(
      this, l10n_util::GetStringUTF16(IDS_OK));
  close_button_->SetIsDefault(true);

  layout->StartRow(0, kColumnSetControls);
  layout->AddView(advanced_link_);
  layout->AddView(close_button_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, 0));
}

void OneClickSigninBubbleView::WindowClosing() {
  // We have to reset |bubble_view_| here, not in our destructor, because
  // we'll be destroyed asynchronously and the shown state will be checked
  // before then.
  DCHECK(bubble_view_ == this);
  bubble_view_ = NULL;
 }

bool OneClickSigninBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN ||
      accelerator.key_code() == ui::VKEY_ESCAPE) {
    StartFade(false);
    return true;
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void OneClickSigninBubbleView::LinkClicked(views::Link* source,
                                           int event_flags) {
  StartFade(false);

  if (source == learn_more_link_)
    learn_more_callback_.Run();
  else
    advanced_callback_.Run();
}

void OneClickSigninBubbleView::ButtonPressed(views::Button* sender,
                                             const views::Event& event) {
  DCHECK_EQ(close_button_, sender);
  StartFade(false);
}
