// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/database_info_view.h"

#include <algorithm>

#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "gfx/color_utils.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

static const int kDatabaseInfoViewBorderSize = 1;
static const int kDatabaseInfoViewInsetSize = 3;

///////////////////////////////////////////////////////////////////////////////
// DatabaseInfoView, public:

DatabaseInfoView::DatabaseInfoView()
    : name_value_field_(NULL),
      description_value_field_(NULL),
      size_value_field_(NULL),
      last_modified_value_field_(NULL) {
}

DatabaseInfoView::~DatabaseInfoView() {
}

void DatabaseInfoView::SetDatabaseInfo(
    const BrowsingDataDatabaseHelper::DatabaseInfo& database_info) {
  name_value_field_->SetText(database_info.database_name.empty() ?
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME)) :
      UTF8ToWide(database_info.database_name));
  description_value_field_->SetText(UTF8ToWide(database_info.description));
  size_value_field_->SetText(
      FormatBytes(database_info.size,
                  GetByteDisplayUnits(database_info.size),
                  true));
  last_modified_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(database_info.last_modified));
  EnableDatabaseDisplay(true);
}

void DatabaseInfoView::EnableDatabaseDisplay(bool enabled) {
  name_value_field_->SetEnabled(enabled);
  description_value_field_->SetEnabled(enabled);
  size_value_field_->SetEnabled(enabled);
  last_modified_value_field_->SetEnabled(enabled);
}

void DatabaseInfoView::ClearDatabaseDisplay() {
  const std::wstring kEmpty;
  description_value_field_->SetText(kEmpty);
  size_value_field_->SetText(kEmpty);
  last_modified_value_field_->SetText(kEmpty);
  EnableDatabaseDisplay(false);
}

///////////////////////////////////////////////////////////////////////////////
// DatabaseInfoView, views::View overrides:

void DatabaseInfoView::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// DatabaseInfoView, private:

void DatabaseInfoView::Init() {
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kDatabaseInfoViewBorderSize, border_color);
  set_border(border);

  views::Label* name_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NAME_LABEL)));
  name_value_field_ = new views::Textfield;
  views::Label* description_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_COOKIES_WEB_DATABASE_DESCRIPTION_LABEL)));
  description_value_field_ = new views::Textfield;
  views::Label* size_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_COOKIES_LOCAL_STORAGE_SIZE_ON_DISK_LABEL)));
  size_value_field_ = new views::Textfield;
  views::Label* last_modified_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_COOKIES_LOCAL_STORAGE_LAST_MODIFIED_LABEL)));
  last_modified_value_field_ = new views::Textfield;

  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kDatabaseInfoViewInsetSize,
                    kDatabaseInfoViewInsetSize,
                    kDatabaseInfoViewInsetSize,
                    kDatabaseInfoViewInsetSize);
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
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(description_label);
  layout->AddView(description_value_field_);
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
  description_value_field_->SetReadOnly(true);
  description_value_field_->RemoveBorder();
  description_value_field_->SetBackgroundColor(text_area_background);
  size_value_field_->SetReadOnly(true);
  size_value_field_->RemoveBorder();
  size_value_field_->SetBackgroundColor(text_area_background);
  last_modified_value_field_->SetReadOnly(true);
  last_modified_value_field_->RemoveBorder();
  last_modified_value_field_->SetBackgroundColor(text_area_background);
}
