// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/database_open_info_view.h"

#include <algorithm>

#include "app/gfx/color_utils.h"
#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "views/grid_layout.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/standard_layout.h"

static const int kDatabaseOpenInfoViewBorderSize = 1;
static const int kDatabaseOpenInfoViewInsetSize = 3;

///////////////////////////////////////////////////////////////////////////////
// DatabaseOpenInfoView, public:

DatabaseOpenInfoView::DatabaseOpenInfoView()
    : host_value_field_(NULL),
      database_name_value_field_(NULL) {
}

DatabaseOpenInfoView::~DatabaseOpenInfoView() {
}

void DatabaseOpenInfoView::SetFields(const std::string& host,
                                     const string16& database_name) {
  host_value_field_->SetText(UTF8ToWide(host));
  database_name_value_field_->SetText(database_name);
  EnableDisplay(true);
}

void DatabaseOpenInfoView::EnableDisplay(bool enabled) {
  host_value_field_->SetEnabled(enabled);
  database_name_value_field_->SetEnabled(enabled);
}

///////////////////////////////////////////////////////////////////////////////
// DatabaseOpenInfoView, views::View overrides:

void DatabaseOpenInfoView::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// DatabaseOpenInfoView, private:

void DatabaseOpenInfoView::Init() {
  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kDatabaseOpenInfoViewBorderSize, border_color);
  set_border(border);

  // TODO(jorlow): These strings are not quite right, but we're post-freeze.
  views::Label* host_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_DOMAIN_LABEL));
  host_value_field_ = new views::Textfield;
  views::Label* database_name_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_NAME_LABEL));
  database_name_value_field_ = new views::Textfield;

  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kDatabaseOpenInfoViewInsetSize,
                    kDatabaseOpenInfoViewInsetSize,
                    kDatabaseOpenInfoViewInsetSize,
                    kDatabaseOpenInfoViewInsetSize);
  SetLayoutManager(layout);

  int three_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, three_column_layout_id);
  layout->AddView(host_label);
  layout->AddView(host_value_field_);
  layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
  layout->StartRow(0, three_column_layout_id);
  layout->AddView(database_name_label);
  layout->AddView(database_name_value_field_);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  host_value_field_->SetReadOnly(true);
  host_value_field_->RemoveBorder();
  host_value_field_->SetBackgroundColor(text_area_background);
  database_name_value_field_->SetReadOnly(true);
  database_name_value_field_->RemoveBorder();
  database_name_value_field_->SetBackgroundColor(text_area_background);
}

