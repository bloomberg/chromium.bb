// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/generic_info_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

GenericInfoView::GenericInfoView(int number_of_rows)
    : number_of_rows_(number_of_rows), name_string_ids_(NULL) {
  DCHECK(number_of_rows_ > 0);
}

GenericInfoView::GenericInfoView(
    int number_of_rows, const int name_string_ids[])
    : number_of_rows_(number_of_rows), name_string_ids_(name_string_ids) {
  DCHECK(number_of_rows_ > 0);
}

void GenericInfoView::SetNameByStringId(int row, int name_string_id) {
  SetName(row, l10n_util::GetStringUTF16(name_string_id));
}

void GenericInfoView::SetName(int row, const string16& name) {
  DCHECK(name_views_.get());  // Can only be called after Init time.
  DCHECK(row >= 0 && row < number_of_rows_);
  name_views_[row]->SetText(name);
}

void GenericInfoView::SetValue(int row, const string16& name) {
  DCHECK(value_views_.get());  // Can only be called after Init time.
  DCHECK(row >= 0 && row < number_of_rows_);
  value_views_[row]->SetText(name);
}

void GenericInfoView::ViewHierarchyChanged(bool is_add,
                                           views::View* parent,
                                           views::View* child) {
  if (is_add && child == this) {
    InitGenericInfoView();
    if (name_string_ids_) {
      for (int i = 0; i < number_of_rows_; ++i)
        SetNameByStringId(i, name_string_ids_[i]);
    }
  }
}

void GenericInfoView::InitGenericInfoView() {
  const int kInfoViewBorderSize = 1;
  const int kInfoViewInsetSize = 3;
  const int kLayoutId = 0;

  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kInfoViewBorderSize, border_color);
  set_border(border);

  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kInfoViewInsetSize, kInfoViewInsetSize,
                    kInfoViewInsetSize, kInfoViewInsetSize);
  SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(kLayoutId);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  name_views_.reset(new views::Label* [number_of_rows_]);
  value_views_.reset(new views::Textfield* [number_of_rows_]);

  for (int i = 0; i < number_of_rows_; ++i) {
    if (i)
      layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
    name_views_[i] = new views::Label;
    value_views_[i] = new views::Textfield;
    AddRow(kLayoutId, layout, name_views_[i], value_views_[i]);
  }
}

void GenericInfoView::AddRow(
    int layout_id, views::GridLayout* layout, views::Label* name,
    views::Textfield* value) {
  // Add to the view hierarchy.
  layout->StartRow(0, layout_id);
  layout->AddView(name);
  layout->AddView(value);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);

  // Init them now that they're in the view hierarchy.
  value->SetReadOnly(true);
  value->RemoveBorder();
  value->SetBackgroundColor(text_area_background);
}
