// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/open_pdf_in_reader_bubble_view.h"

#include "chrome/browser/ui/pdf/open_pdf_in_reader_prompt_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

OpenPDFInReaderBubbleView::~OpenPDFInReaderBubbleView() {}

OpenPDFInReaderBubbleView::OpenPDFInReaderBubbleView(
    views::View* anchor_view,
    OpenPDFInReaderPromptDelegate* model)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT),
      model_(model),
      open_in_reader_link_(NULL),
      close_button_(NULL) {
  DCHECK(model);
}

void OpenPDFInReaderBubbleView::Init() {
  using views::GridLayout;

  GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  string16 title = model_->GetMessageText();
  views::Label* title_label = new views::Label(title);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  string16 accept_text = model_->GetAcceptButtonText();
  open_in_reader_link_ = new views::Link(accept_text);
  open_in_reader_link_->SetEnabled(true);
  open_in_reader_link_->set_listener(this);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(open_in_reader_link_);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(new views::Separator(views::Separator::HORIZONTAL), 1, 1,
                  GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  close_button_ = new views::LabelButton(this, model_->GetCancelButtonText());
  close_button_->SetStyle(views::Button::STYLE_NATIVE_TEXTBUTTON);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(close_button_);
}

void OpenPDFInReaderBubbleView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  DCHECK_EQ(close_button_, sender);

  model_->Cancel();
  StartFade(false);
}

void OpenPDFInReaderBubbleView::LinkClicked(views::Link* source,
                                            int event_flags) {
  DCHECK_EQ(open_in_reader_link_, source);

  model_->Accept();
  StartFade(false);
}

