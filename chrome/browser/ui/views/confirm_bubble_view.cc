// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_view.h"

#include "chrome/browser/ui/confirm_bubble_model.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {

// Maximum width for the message field. We will wrap the message text when its
// width is wider than this.
const int kMaxMessageWidth = 400;

}  // namespace

void ConfirmBubbleModel::Show(gfx::NativeView view,
                              const gfx::Point& origin,
                              ConfirmBubbleModel* model) {
  ConfirmBubbleView* bubble_view = new ConfirmBubbleView(origin, model);
  views::BubbleDelegateView::CreateBubble(bubble_view);
  bubble_view->Show();
}

ConfirmBubbleView::ConfirmBubbleView(const gfx::Point& anchor_point,
                                     ConfirmBubbleModel* model)
    : BubbleDelegateView(NULL, views::BubbleBorder::NONE),
      anchor_point_(anchor_point),
      model_(model) {
  DCHECK(model);
}

ConfirmBubbleView::~ConfirmBubbleView() {
}

void ConfirmBubbleView::ButtonPressed(views::Button* sender,
                                      const views::Event& event) {
  if (sender->tag() == ConfirmBubbleModel::BUTTON_OK)
    model_->Accept();
  else if (sender->tag() == ConfirmBubbleModel::BUTTON_CANCEL)
    model_->Cancel();
  GetWidget()->Close();
}

void ConfirmBubbleView::LinkClicked(views::Link* source, int event_flags) {
  model_->LinkClicked();
}

gfx::Rect ConfirmBubbleView::GetAnchorRect() {
  return gfx::Rect(anchor_point_, gfx::Size());
}

void ConfirmBubbleView::Init() {
  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Add the icon, the title label and the close button to the first row.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  gfx::Image* icon_image = model_->GetIcon();
  DCHECK(icon_image);
  views::ImageView* icon_view = new views::ImageView;
  icon_view->SetImage(icon_image->ToImageSkia());
  layout->AddView(icon_view);

  const string16 title_text = model_->GetTitle();
  DCHECK(!title_text.empty());
  views::Label* title_label = new views::Label(title_text);
  title_label->SetFont(bundle.GetFont(ui::ResourceBundle::MediumFont));
  layout->AddView(title_label);

  views::ImageButton* close_button = new views::ImageButton(this);
  const gfx::ImageSkia* close_image =
      bundle.GetImageNamed(IDR_INFO_BUBBLE_CLOSE).ToImageSkia();
  close_button->SetImage(views::CustomButton::BS_NORMAL, close_image);
  close_button->set_tag(ConfirmBubbleModel::BUTTON_NONE);
  layout->AddView(close_button);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // Add the message label to the second row.
  cs = layout->AddColumnSet(1);
  const string16 message_text = model_->GetMessageText();
  DCHECK(!message_text.empty());
  views::Label* message_label = new views::Label(message_text);
  int message_width =
      std::min(kMaxMessageWidth, message_label->GetPreferredSize().width());
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::FIXED, message_width, false);
  message_label->SetBounds(0, 0, message_width, 0);
  message_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  message_label->SetMultiLine(true);
  layout->StartRow(0, 1);
  layout->AddView(message_label);

  // Add the the link label to the third row if it exists.
  const string16 link_text = model_->GetLinkText();
  if (!link_text.empty()) {
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    layout->StartRow(0, 1);
    views::Link* link_label = new views::Link(link_text);
    link_label->set_listener(this);
    layout->AddView(link_label);
  }
  layout->AddPaddingRow(0, views::kLabelToControlVerticalSpacing);

  // Add the ok button and the cancel button to the third (or fourth) row if we
  // have either of them.
  bool has_ok_button = !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_OK);
  bool has_cancel_button =
      !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_CANCEL);
  if (has_ok_button || has_cancel_button) {
    cs = layout->AddColumnSet(2);
    cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
    if (has_ok_button) {
      cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                    views::GridLayout::USE_PREF, 0, 0);
      if (has_cancel_button)
        cs->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
    }
    if (has_cancel_button) {
      cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::CENTER, 0,
                    views::GridLayout::USE_PREF, 0, 0);
    }
    layout->StartRow(0, 2);
    if (has_ok_button) {
      views::TextButton* ok_button = new views::NativeTextButton(
          this, model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_OK));
      ok_button->set_tag(ConfirmBubbleModel::BUTTON_OK);
      layout->AddView(ok_button);
    }
    if (has_cancel_button) {
      views::TextButton* cancel_button = new views::NativeTextButton(
          this, model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_CANCEL));
      cancel_button->set_tag(ConfirmBubbleModel::BUTTON_CANCEL);
      layout->AddView(cancel_button);
    }
  }
}
