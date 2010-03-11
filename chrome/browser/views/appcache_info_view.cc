// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/appcache_info_view.h"

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

///////////////////////////////////////////////////////////////////////////////
// AppCacheInfoView, public:

AppCacheInfoView::AppCacheInfoView()
    : manifest_url_field_(NULL),
      size_field_(NULL),
      creation_date_field_(NULL),
      last_access_field_(NULL) {
}

AppCacheInfoView::~AppCacheInfoView() {
}

void AppCacheInfoView::SetAppCacheInfo(const appcache::AppCacheInfo* info) {
  DCHECK(info);
  manifest_url_field_->SetText(UTF8ToWide(info->manifest_url.spec()));
  size_field_->SetText(
      FormatBytes(info->size, GetByteDisplayUnits(info->size), true));
  creation_date_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(info->creation_time));
  last_access_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(info->last_access_time));
  EnableAppCacheDisplay(true);
}

void AppCacheInfoView::EnableAppCacheDisplay(bool enabled) {
  manifest_url_field_->SetEnabled(enabled);
  size_field_->SetEnabled(enabled);
  creation_date_field_->SetEnabled(enabled);
  last_access_field_->SetEnabled(enabled);
}

void AppCacheInfoView::ClearAppCacheDisplay() {
  const string16 kEmpty;
  manifest_url_field_->SetText(kEmpty);
  size_field_->SetText(kEmpty);
  creation_date_field_->SetText(kEmpty);
  last_access_field_->SetText(kEmpty);
  EnableAppCacheDisplay(false);
}

///////////////////////////////////////////////////////////////////////////////
// AppCacheInfoView, views::View overrides:

void AppCacheInfoView::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// AppCacheInfoView, private:


void AppCacheInfoView::Init() {
  const int kInfoViewBorderSize = 1;
  const int kInfoViewInsetSize = 3;
  const int kLayoutId = 0;

  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kInfoViewBorderSize, border_color);
  set_border(border);

  views::Label* manifest_url_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_APPLICATION_CACHE_MANIFEST_LABEL));
  manifest_url_field_ = new views::Textfield;
  views::Label* size_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_SIZE_LABEL));
  size_field_ = new views::Textfield;
  views::Label* creation_date_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_COOKIE_CREATED_LABEL));
  creation_date_field_ = new views::Textfield;
  views::Label* last_access_label = new views::Label(
      l10n_util::GetString(IDS_COOKIES_LAST_ACCESSED_LABEL));
  last_access_field_ = new views::Textfield;

  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kInfoViewInsetSize, kInfoViewInsetSize,
                    kInfoViewInsetSize, kInfoViewInsetSize);
  SetLayoutManager(layout);

  views::ColumnSet* column_set = layout->AddColumnSet(kLayoutId);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  AddRow(kLayoutId, layout, manifest_url_label, manifest_url_field_, true);
  AddRow(kLayoutId, layout, size_label, size_field_, true);
  AddRow(kLayoutId, layout, creation_date_label, creation_date_field_, true);
  AddRow(kLayoutId, layout, last_access_label, last_access_field_, false);
}

void AppCacheInfoView::AddRow(
    int layout_id, views::GridLayout* layout, views::Label* label,
    views::Textfield* field, bool add_padding_row) {
  // Add to the view hierarchy.
  layout->StartRow(0, layout_id);
  layout->AddView(label);
  layout->AddView(field);

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);

  // Init them now that they're in the view heirarchy.
  field->SetReadOnly(true);
  field->RemoveBorder();
  field->SetBackgroundColor(text_area_background);

  if (add_padding_row)
    layout->AddPaddingRow(0, kRelatedControlSmallVerticalSpacing);
}
