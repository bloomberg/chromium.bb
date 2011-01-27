// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vsstyle.h>
#include <vssym32.h>

#include "chrome/browser/ui/views/options/options_group_view.h"

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas.h"
#include "gfx/font.h"
#include "gfx/native_theme_win.h"
#include "grit/locale_settings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"

static const int kLeftColumnWidthChars = 20;
static const int kOptionsGroupViewColumnSpacing = 30;

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupView, public:

OptionsGroupView::OptionsGroupView(views::View* contents,
                                   const std::wstring& title,
                                   const std::wstring& description,
                                   bool show_separator)
    : contents_(contents),
      title_label_(new views::Label(title)),
      description_label_(new views::Label(description)),
      separator_(NULL),
      show_separator_(show_separator),
      highlighted_(false) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& title_font =
      rb.GetFont(ResourceBundle::BaseFont).DeriveFont(0, gfx::Font::BOLD);
  title_label_->SetFont(title_font);
  SkColor title_color = gfx::NativeTheme::instance()->GetThemeColorWithDefault(
      gfx::NativeTheme::BUTTON, BP_GROUPBOX, GBS_NORMAL, TMT_TEXTCOLOR,
      COLOR_WINDOWTEXT);
  title_label_->SetColor(title_color);
  title_label_->SetMultiLine(true);
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  description_label_->SetMultiLine(true);
  description_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  SetAccessibleName(WideToUTF16Hack(title));
  contents->SetAccessibleName(WideToUTF16Hack(title));
}

OptionsGroupView::~OptionsGroupView() {
}

void OptionsGroupView::SetHighlighted(bool highlighted) {
  highlighted_ = highlighted;
  SchedulePaint();
}

int OptionsGroupView::GetContentsWidth() const {
  return contents_->width();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupView, views::View overrides:

AccessibilityTypes::Role OptionsGroupView::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_GROUPING;
}

void OptionsGroupView::Paint(gfx::Canvas* canvas) {
  if (highlighted_) {
    COLORREF infocolor = GetSysColor(COLOR_INFOBK);
    SkColor background_color = SkColorSetRGB(GetRValue(infocolor),
                                             GetGValue(infocolor),
                                             GetBValue(infocolor));
    int y_offset = kUnrelatedControlVerticalSpacing / 2;
    canvas->FillRectInt(background_color, 0, 0, width(),
                        height() - y_offset);
  }
}

void OptionsGroupView::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// OptionsGroupView, private:

void OptionsGroupView::Init() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& font = rb.GetFont(ResourceBundle::BaseFont);
  int left_column_chars = 0;
  base::StringToInt(
      l10n_util::GetStringUTF16(IDS_OPTIONS_DIALOG_LEFT_COLUMN_WIDTH_CHARS),
      &left_column_chars);
  int left_column_width = font.GetExpectedTextWidth(left_column_chars);

  const int two_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(two_column_layout_id);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::FIXED, left_column_width, 0);
  column_set->AddPaddingColumn(0, kOptionsGroupViewColumnSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);

  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, two_column_layout_id);
  layout->AddView(title_label_, 1, 1, GridLayout::FILL, GridLayout::LEADING);
  layout->AddView(contents_, 1, 3, GridLayout::FILL, GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(1, two_column_layout_id);
  layout->AddView(description_label_, 1, 1,
                  GridLayout::FILL, GridLayout::LEADING);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  if (show_separator_) {
    const int single_column_layout_id = 1;
    column_set = layout->AddColumnSet(single_column_layout_id);
    column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                          GridLayout::USE_PREF, 0, 0);

    separator_ = new views::Separator;
    layout->StartRow(0, single_column_layout_id);
    layout->AddView(separator_);
  }
}
