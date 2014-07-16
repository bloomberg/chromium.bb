// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/confirm_bubble_views.h"

#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

ConfirmBubbleViews::ConfirmBubbleViews(ConfirmBubbleModel* model)
    : model_(model),
      link_(NULL) {
  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Use a fixed maximum message width, so longer messages will wrap.
  const int kMaxMessageWidth = 400;
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                views::GridLayout::FIXED, kMaxMessageWidth, false);

  // Add the message label.
  views::Label* label = new views::Label(model_->GetMessageText());
  DCHECK(!label->text().empty());
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMultiLine(true);
  label->SizeToFit(kMaxMessageWidth);
  layout->StartRow(0, 0);
  layout->AddView(label);

  // Initialize the link.
  link_ = new views::Link(model_->GetLinkText());
  link_->set_listener(this);
  link_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
}

ConfirmBubbleViews::~ConfirmBubbleViews() {
}

base::string16 ConfirmBubbleViews::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_OK);
    case ui::DIALOG_BUTTON_CANCEL:
      return model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_CANCEL);
    default:
      NOTREACHED();
      return DialogDelegateView::GetDialogButtonLabel(button);
  }
}

bool ConfirmBubbleViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_OK);
    case ui::DIALOG_BUTTON_CANCEL:
      return !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_CANCEL);
    default:
      NOTREACHED();
      return false;
  }
}

views::View* ConfirmBubbleViews::CreateExtraView() {
  return link_;
}

bool ConfirmBubbleViews::Cancel() {
  model_->Cancel();
  return true;
}

bool ConfirmBubbleViews::Accept() {
  model_->Accept();
  return true;
}

ui::ModalType ConfirmBubbleViews::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 ConfirmBubbleViews::GetWindowTitle() const {
  return model_->GetTitle();
}

void ConfirmBubbleViews::LinkClicked(views::Link* source, int event_flags) {
  if (source == link_) {
    model_->LinkClicked();
    GetWidget()->Close();
  }
}

namespace chrome {

void ShowConfirmBubble(gfx::NativeWindow window,
                       gfx::NativeView anchor_view,
                       const gfx::Point& origin,
                       ConfirmBubbleModel* model) {
  CreateBrowserModalDialogViews(new ConfirmBubbleViews(model), window)->Show();
}

}  // namespace chrome
