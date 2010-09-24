// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/indexed_db_info_view.h"

#include <algorithm>

#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "gfx/color_utils.h"
#include "grit/generated_resources.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"

static const int kIndexedDBInfoViewBorderSize = 1;
static const int kIndexedDBInfoViewInsetSize = 3;

///////////////////////////////////////////////////////////////////////////////
// IndexedDBInfoView, public:

IndexedDBInfoView::IndexedDBInfoView()
    : name_value_field_(NULL),
      origin_value_field_(NULL),
      size_value_field_(NULL),
      last_modified_value_field_(NULL) {
}

IndexedDBInfoView::~IndexedDBInfoView() {
}

void IndexedDBInfoView::SetIndexedDBInfo(
    const BrowsingDataIndexedDBHelper::IndexedDBInfo& indexed_db_info) {
  name_value_field_->SetText(
      indexed_db_info.database_name.empty() ?
          l10n_util::GetString(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME) :
          UTF8ToWide(indexed_db_info.database_name));
  origin_value_field_->SetText(UTF8ToWide(indexed_db_info.origin));
  size_value_field_->SetText(
      FormatBytes(indexed_db_info.size,
                  GetByteDisplayUnits(indexed_db_info.size),
                  true));
  last_modified_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(indexed_db_info.last_modified));
  EnableIndexedDBDisplay(true);
}

void IndexedDBInfoView::EnableIndexedDBDisplay(bool enabled) {
  name_value_field_->SetEnabled(enabled);
  origin_value_field_->SetEnabled(enabled);
  size_value_field_->SetEnabled(enabled);
  last_modified_value_field_->SetEnabled(enabled);
}

void IndexedDBInfoView::ClearIndexedDBDisplay() {
  std::wstring no_cookie_string =
      l10n_util::GetString(IDS_COOKIES_COOKIE_NONESELECTED);
  name_value_field_->SetText(no_cookie_string);
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

  views::Label* name_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_NAME_LABEL));
  name_value_field_ = new views::Textfield;
  views::Label* origin_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LOCAL_STORAGE_ORIGIN_LABEL));
  origin_value_field_ = new views::Textfield;
  views::Label* size_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL));
  size_value_field_ = new views::Textfield;
  views::Label* last_modified_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL));
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
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, three_column_layout_id);
  layout->AddView(name_label);
  layout->AddView(name_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(origin_label);
  layout->AddView(origin_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(size_label);
  layout->AddView(size_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(last_modified_label);
  layout->AddView(last_modified_value_field_);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  name_value_field_->SetReadOnly(true);
  name_value_field_->RemoveBorder();
  name_value_field_->SetBackgroundColor(text_area_background);
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
