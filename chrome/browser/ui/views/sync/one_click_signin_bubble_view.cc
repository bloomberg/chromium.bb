// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sync/one_click_signin_bubble_view.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/browser.h"
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

// Minimum width for the mutli-line label.
const int kMinimumLabelWidth = 240;


// OneClickSigninBubbleView ----------------------------------------------------

// static
OneClickSigninBubbleView* OneClickSigninBubbleView::bubble_view_ = NULL;

// static
void OneClickSigninBubbleView::ShowBubble(
    views::View* anchor_view,
    const BrowserWindow::StartSyncCallback& start_sync) {
  if (IsShowing())
    return;

  bubble_view_ =
      new OneClickSigninBubbleView(anchor_view, start_sync);
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
    const BrowserWindow::StartSyncCallback& start_sync_callback)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      advanced_link_(NULL),
      ok_button_(NULL),
      undo_button_(NULL),
      start_sync_callback_(start_sync_callback),
      message_loop_for_testing_(NULL) {
  DCHECK(!start_sync_callback_.is_null());
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
                views::GridLayout::USE_PREF, 0, kMinimumLabelWidth);

  cs = layout->AddColumnSet(kColumnSetControls);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kUnrelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);

  // Add main text description.
  views::Label* label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_SIGNIN_BUBBLE_MESSAGE));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label->SizeToFit(kMinimumLabelWidth);

  layout->StartRow(0, kColumnSetFillAlign);
  layout->AddView(label);

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);

  // Add link for user to do advanced config of sync.
  advanced_link_= new views::Link(
      l10n_util::GetStringUTF16(IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  advanced_link_->set_listener(this);
  advanced_link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // Add controls at the bottom.
  ok_button_ = new views::NativeTextButton(this);
  ok_button_->SetIsDefault(true);

  undo_button_ = new views::NativeTextButton(this);

  // The default size of the buttons is too large.  To allow them to be smaller
  // ignore the minimum default size.  Furthermore, to make sure they are the
  // same size, SetText() is called with both strings on both buttons.
  ok_button_->set_ignore_minimum_size(true);
  undo_button_->set_ignore_minimum_size(true);
  string16 ok_label = l10n_util::GetStringUTF16(IDS_OK);
  string16 undo_label = l10n_util::GetStringUTF16(IDS_ONE_CLICK_BUBBLE_UNDO);
  ok_button_->SetText(undo_label);
  ok_button_->SetText(ok_label);
  undo_button_->SetText(ok_label);
  undo_button_->SetText(undo_label);

  layout->StartRow(0, kColumnSetControls);
  layout->AddView(advanced_link_);
  layout->AddView(ok_button_);
  layout->AddView(undo_button_);

  AddAccelerator(ui::Accelerator(ui::VKEY_RETURN, 0));
}

void OneClickSigninBubbleView::WindowClosing() {
  // We have to reset |bubble_view_| here, not in our destructor, because
  // we'll be destroyed asynchronously and the shown state will be checked
  // before then.
  DCHECK(bubble_view_ == this);
  bubble_view_ = NULL;

  if (!start_sync_callback_.is_null()) {
    base::ResetAndReturn(&start_sync_callback_).Run(
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }
}

bool OneClickSigninBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (accelerator.key_code() == ui::VKEY_RETURN ||
      accelerator.key_code() == ui::VKEY_ESCAPE) {
    StartFade(false);
    if (accelerator.key_code() == ui::VKEY_RETURN) {
      base::ResetAndReturn(&start_sync_callback_).Run(
          OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
    } else {
      start_sync_callback_.Reset();
    }

    return true;
  }

  return BubbleDelegateView::AcceleratorPressed(accelerator);
}

void OneClickSigninBubbleView::LinkClicked(views::Link* source,
                                           int event_flags) {
  StartFade(false);
  base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
}

void OneClickSigninBubbleView::ButtonPressed(views::Button* sender,
                                             const views::Event& event) {
  StartFade(false);
  if (ok_button_ == sender) {
    base::ResetAndReturn(&start_sync_callback_).Run(
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  } else {
    start_sync_callback_.Reset();
  }
}
