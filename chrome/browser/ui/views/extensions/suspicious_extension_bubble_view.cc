// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/suspicious_extension_bubble_view.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/suspicious_extension_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "grit/locale_settings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// Layout constants.
const int kExtensionListPadding = 10;
const int kInsetBottomRight = 13;
const int kInsetLeft = 14;
const int kInsetTop = 9;
const int kHeadlineMessagePadding = 4;
const int kHeadlineRowPadding = 10;
const int kMessageBubblePadding = 11;

// How many extensions to show in the bubble (max).
const size_t kMaxExtensionsToShow = 7;

// How long to wait until showing the bubble (in seconds).
const int kBubbleAppearanceWaitTime = 5;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleView

namespace extensions {

SuspiciousExtensionBubbleView::SuspiciousExtensionBubbleView(
    views::View* anchor_view,
    SuspiciousExtensionBubbleController* controller)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      weak_factory_(this),
      controller_(controller),
      headline_(NULL),
      learn_more_(NULL),
      dismiss_button_(NULL),
      link_clicked_(false) {
  DCHECK(anchor_view->GetWidget());
  set_close_on_deactivate(false);
  set_move_with_anchor(true);
  set_close_on_esc(true);

  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
}

// static
void SuspiciousExtensionBubbleView::MaybeShow(
    Browser* browser,
    views::View* anchor_view) {
  SuspiciousExtensionBubbleController* controller =
      extensions::SuspiciousExtensionBubbleController::Get(
          browser->profile());
  if (controller->HasSuspiciousExtensions()) {
    SuspiciousExtensionBubbleView* bubble_delegate =
        new SuspiciousExtensionBubbleView(anchor_view, controller);
    views::BubbleDelegateView::CreateBubble(bubble_delegate);
    controller->Show(bubble_delegate);
  }
}

void SuspiciousExtensionBubbleView::OnButtonClicked(
    const base::Closure& callback) {
  button_callback_ = callback;
}

void SuspiciousExtensionBubbleView::OnLinkClicked(
    const base::Closure& callback) {
  link_callback_ = callback;
}

void SuspiciousExtensionBubbleView::Show() {
  // Not showing the bubble right away (during startup) has a few benefits:
  // We don't have to worry about focus being lost due to the Omnibox (or to
  // other things that want focus at startup). This allows Esc to work to close
  // the bubble and also solves the keyboard accessibility problem that comes
  // with focus being lost (we don't have a good generic mechanism of injecting
  // bubbles into the focus cycle). Another benefit of delaying the show is
  // that fade-in works (the fade-in isn't apparent if the the bubble appears at
  // startup).
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&SuspiciousExtensionBubbleView::ShowBubble,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kBubbleAppearanceWaitTime));
}

void SuspiciousExtensionBubbleView::OnWidgetDestroying(views::Widget* widget) {
  // To catch Esc, we monitor destroy message. Unless the link has been clicked,
  // we assume Dismiss was the action taken.
  if (!link_clicked_)
    button_callback_.Run();
}

////////////////////////////////////////////////////////////////////////////////
// SuspiciousExtensionBubbleView - private.

SuspiciousExtensionBubbleView::~SuspiciousExtensionBubbleView() {
}

void SuspiciousExtensionBubbleView::ShowBubble() {
  StartFade(true);
}

void SuspiciousExtensionBubbleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  layout->SetInsets(kInsetTop, kInsetLeft,
                    kInsetBottomRight, kInsetBottomRight);
  SetLayoutManager(layout);

  const int headline_column_set_id = 0;
  views::ColumnSet* top_columns = layout->AddColumnSet(headline_column_set_id);
  top_columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                         0, views::GridLayout::USE_PREF, 0, 0);
  top_columns->AddPaddingColumn(1, 0);
  layout->StartRow(0, headline_column_set_id);

  headline_ = new views::Label();
  headline_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
  headline_->SetText(controller_->GetTitle());
  layout->AddView(headline_);

  layout->AddPaddingRow(0, kHeadlineRowPadding);

  const int text_column_set_id = 1;
  views::ColumnSet* upper_columns = layout->AddColumnSet(text_column_set_id);
  upper_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::LEADING,
      0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, text_column_set_id);

  views::Label* message = new views::Label();
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetText(controller_->GetMessageBody());
  message->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_EXTENSION_WIPEOUT_BUBBLE_WIDTH_CHARS));
  layout->AddView(message);

  const int extension_list_column_set_id = 2;
  views::ColumnSet* middle_columns =
      layout->AddColumnSet(extension_list_column_set_id);
  middle_columns->AddPaddingColumn(0, kExtensionListPadding);
  middle_columns->AddColumn(
      views::GridLayout::LEADING, views::GridLayout::CENTER,
      0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRowWithPadding(0, extension_list_column_set_id,
      0, kHeadlineMessagePadding);
  views::Label* extensions = new views::Label();
  extensions->SetMultiLine(true);
  extensions->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  std::vector<base::string16> extension_list;
  char16 bullet_point = 0x2022;

  std::vector<base::string16> suspicious =
      controller_->GetSuspiciousExtensionNames();
  size_t i = 0;
  for (; i < suspicious.size() && i < kMaxExtensionsToShow; ++i) {
    // Add each extension with bullet point.
    extension_list.push_back(bullet_point + ASCIIToUTF16(" ") + suspicious[i]);
  }

  if (i > kMaxExtensionsToShow) {
    base::string16 difference = base::IntToString16(i - kMaxExtensionsToShow);
    extension_list.push_back(bullet_point + ASCIIToUTF16(" ") +
        controller_->GetOverflowText(difference));
  }

  extensions->SetText(JoinString(extension_list, ASCIIToUTF16("\n")));
  extensions->SizeToFit(views::Widget::GetLocalizedContentsWidth(
      IDS_EXTENSION_WIPEOUT_BUBBLE_WIDTH_CHARS));
  layout->AddView(extensions);

  const int action_row_column_set_id = 3;
  views::ColumnSet* bottom_columns =
      layout->AddColumnSet(action_row_column_set_id);
  bottom_columns->AddColumn(views::GridLayout::LEADING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  bottom_columns->AddPaddingColumn(1, 0);
  bottom_columns->AddColumn(views::GridLayout::TRAILING,
      views::GridLayout::CENTER, 0, views::GridLayout::USE_PREF, 0, 0);
  layout->StartRowWithPadding(0, action_row_column_set_id,
                              0, kMessageBubblePadding);

  learn_more_ = new views::Link(controller_->GetLearnMoreLabel());
  learn_more_->set_listener(this);
  layout->AddView(learn_more_);

  dismiss_button_ = new views::LabelButton(this,
      controller_->GetDismissButtonLabel());
  dismiss_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  layout->AddView(dismiss_button_);
}

void SuspiciousExtensionBubbleView::ButtonPressed(views::Button* sender,
                                                  const ui::Event& event) {
  DCHECK_EQ(dismiss_button_, sender);
  GetWidget()->Close();
}

void SuspiciousExtensionBubbleView::LinkClicked(views::Link* source,
                                                int event_flags) {
  DCHECK_EQ(learn_more_, source);
  link_clicked_ = true;
  link_callback_.Run();
  GetWidget()->Close();
}

void SuspiciousExtensionBubbleView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_ALERT;
}

void SuspiciousExtensionBubbleView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    NotifyAccessibilityEvent(ui::AccessibilityTypes::EVENT_ALERT, true);
}

}  // namespace extensions
