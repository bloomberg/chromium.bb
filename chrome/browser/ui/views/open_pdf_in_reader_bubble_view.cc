// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/open_pdf_in_reader_bubble_view.h"

#include "components/pdf/browser/open_pdf_in_reader_prompt_client.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

OpenPDFInReaderBubbleView::~OpenPDFInReaderBubbleView() {}

OpenPDFInReaderBubbleView::OpenPDFInReaderBubbleView(
    views::View* anchor_view,
    pdf::OpenPDFInReaderPromptClient* model)
    : views::BubbleDialogDelegateView(anchor_view,
                                      views::BubbleBorder::TOP_RIGHT),
      model_(model),
      open_in_reader_link_(nullptr) {
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

  base::string16 title = model_->GetMessageText();
  views::Label* title_label = new views::Label(title);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);

  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  base::string16 accept_text = model_->GetAcceptButtonText();
  open_in_reader_link_ = new views::Link(accept_text);
  open_in_reader_link_->set_listener(this);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(open_in_reader_link_);
}

int OpenPDFInReaderBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

base::string16 OpenPDFInReaderBubbleView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  return model_->GetCancelButtonText();
}

bool OpenPDFInReaderBubbleView::Cancel() {
  model_->Cancel();
  return true;
}

void OpenPDFInReaderBubbleView::LinkClicked(views::Link* source,
                                            int event_flags) {
  DCHECK_EQ(open_in_reader_link_, source);

  model_->Accept();
  GetWidget()->Close();
}

