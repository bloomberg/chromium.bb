// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_filter_page_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

static const int kAllowRadioGroup = 1;

ContentFilterPageView::ContentFilterPageView(Profile* profile,
                                             int label_message_id,
                                             int allow_radio_message_id,
                                             int block_radio_message_id)
    : label_message_id_(label_message_id),
      allow_radio_message_id_(allow_radio_message_id),
      block_radio_message_id_(block_radio_message_id),
      caption_label_(NULL),
      allow_radio_(NULL),
      block_radio_(NULL),
      exceptions_button_(NULL),
      OptionsPageView(profile) {
}

ContentFilterPageView::~ContentFilterPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView, views::ButtonListener implementation:

void ContentFilterPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == allow_radio_ || sender == block_radio_)
    OnAllowedChanged(allow_radio_->checked());
  else if (sender == exceptions_button_)
    OnShowExceptionsDialog();
}

////////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView, OptionsPageView implementation:
void ContentFilterPageView::InitControlLayout() {
  // Make sure we don't leak memory by calling this more than once.
  DCHECK(!exceptions_button_);
  using views::GridLayout;
  using views::ColumnSet;

  exceptions_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_EXCEPTIONS_BUTTON));
  caption_label_ = new views::Label(
      l10n_util::GetString(label_message_id_));
  caption_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  caption_label_->SetMultiLine(true);

  allow_radio_ = new views::RadioButton(
      l10n_util::GetString(allow_radio_message_id_),
      kAllowRadioGroup);
  allow_radio_->set_listener(this);
  allow_radio_->SetMultiLine(true);
  block_radio_ = new views::RadioButton(
      l10n_util::GetString(block_radio_message_id_),
      kAllowRadioGroup);
  block_radio_->set_listener(this);
  block_radio_->SetMultiLine(true);


  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int label_view_set_id = 0;
  ColumnSet* label_column_set = layout->AddColumnSet(label_view_set_id);
  label_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  label_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                              GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, label_view_set_id);
  layout->AddView(caption_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int radio_view_set_id = 1;
  ColumnSet* radio_column_set = layout->AddColumnSet(radio_view_set_id);
  radio_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  radio_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                              GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, radio_view_set_id);
  layout->AddView(allow_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, radio_view_set_id);
  layout->AddView(block_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int button_view_set_id = 2;
  ColumnSet* button_column_set = layout->AddColumnSet(button_view_set_id);
  button_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  button_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                               GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, button_view_set_id);
  layout->AddView(exceptions_button_);
}

void ContentFilterPageView::Layout() {
  views::View* parent = GetParent();
  if (parent && parent->width()) {
    const int width = parent->width();
    const int height = GetHeightForWidth(width);
    SetBounds(0, 0, width, height);
  } else {
    gfx::Size prefsize = GetPreferredSize();
    SetBounds(0, 0, prefsize.width(), prefsize.height());
  }
  View::Layout();
}

