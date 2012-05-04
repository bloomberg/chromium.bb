// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/indexed_db_info_view.h"

#include <algorithm>

#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

static const int kIndexedDBInfoViewBorderSize = 1;
static const int kIndexedDBInfoViewInsetSize = 3;

///////////////////////////////////////////////////////////////////////////////
// IndexedDBInfoView, public:

IndexedDBInfoView::IndexedDBInfoView()
    : origin_value_field_(NULL),
      size_value_field_(NULL),
      last_modified_value_field_(NULL) {
}

IndexedDBInfoView::~IndexedDBInfoView() {
}

void IndexedDBInfoView::SetIndexedDBInfo(
    const BrowsingDataIndexedDBHelper::IndexedDBInfo& indexed_db_info) {
  origin_value_field_->SetText(UTF8ToUTF16(indexed_db_info.origin.spec()));
  size_value_field_->SetText(ui::FormatBytes(indexed_db_info.size));
  last_modified_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(indexed_db_info.last_modified));
  EnableIndexedDBDisplay(true);
}

void IndexedDBInfoView::EnableIndexedDBDisplay(bool enabled) {
  origin_value_field_->SetEnabled(enabled);
  size_value_field_->SetEnabled(enabled);
  last_modified_value_field_->SetEnabled(enabled);
}

void IndexedDBInfoView::ClearIndexedDBDisplay() {
  const string16 no_cookie_string =
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NONESELECTED);
  origin_value_field_->SetText(no_cookie_string);
  size_value_field_->SetText(no_cookie_string);
  last_modified_value_field_->SetText(no_cookie_string);
  EnableIndexedDBDisplay(false);
}

///////////////////////////////////////////////////////////////////////////////
// IndexedDBInfoView, views::View overrides:

void IndexedDBInfoView::ViewHierarchyChanged(bool is_add,
                                             views::View* parent,
                                             views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// IndexedDBInfoView, private:

void IndexedDBInfoView::Init() {
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kIndexedDBInfoViewBorderSize, border_color);
  set_border(border);

  views::Label* origin_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL));
  origin_value_field_ = new views::Textfield;
  views::Label* size_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL));
  size_value_field_ = new views::Textfield;
  views::Label* last_modified_label = new views::Label(
      l10n_util::GetStringUTF16(
          IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL));
  last_modified_value_field_ = new views::Textfield;

  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kIndexedDBInfoViewInsetSize,
                    kIndexedDBInfoViewInsetSize,
                    kIndexedDBInfoViewInsetSize,
                    kIndexedDBInfoViewInsetSize);
  SetLayoutManager(layout);

  int three_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, three_column_layout_id);
  layout->AddView(origin_label);
  layout->AddView(origin_value_field_);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(size_label);
  layout->AddView(size_value_field_);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(last_modified_label);
  layout->AddView(last_modified_value_field_);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  origin_value_field_->SetReadOnly(true);
  origin_value_field_->RemoveBorder();
  origin_value_field_->SetBackgroundColor(text_area_background);
  size_value_field_->SetReadOnly(true);
  size_value_field_->RemoveBorder();
  size_value_field_->SetBackgroundColor(text_area_background);
  last_modified_value_field_->SetReadOnly(true);
  last_modified_value_field_->RemoveBorder();
  last_modified_value_field_->SetBackgroundColor(text_area_background);
}
