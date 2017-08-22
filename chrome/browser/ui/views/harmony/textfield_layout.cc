// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/harmony/textfield_layout.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_provider.h"

using views::GridLayout;

namespace {

// GridLayout "resize percent" constants.
constexpr float kFixed = 0.f;
constexpr float kStretchy = 1.f;

}  // namespace

views::ColumnSet* ConfigureTextfieldStack(GridLayout* layout,
                                          int column_set_id) {
  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int between_padding =
      provider->GetDistanceMetric(views::DISTANCE_RELATED_CONTROL_HORIZONTAL);

  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(provider->GetControlLabelGridAlignment(),
                        GridLayout::CENTER, kFixed, GridLayout::USE_PREF, 0, 0);
  // TODO(tapted): This column may need some additional alignment logic under
  // Harmony so that its x-offset is not wholly dictated by the string length
  // of labels in the first column.
  column_set->AddPaddingColumn(kFixed, between_padding);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, kStretchy,
                        GridLayout::USE_PREF, 0, 0);
  return column_set;
}

views::Textfield* AddFirstTextfieldRow(GridLayout* layout,
                                       const base::string16& label_text,
                                       int column_set_id) {
  views::Textfield* textfield = new views::Textfield();
  constexpr int font_context = views::style::CONTEXT_LABEL;
  constexpr int font_style = views::style::STYLE_PRIMARY;
  layout->StartRow(kFixed, column_set_id,
                   views::LayoutProvider::GetControlHeightForFont(
                       font_context, font_style, textfield->GetFontList()));
  views::Label* label = new views::Label(label_text, font_context, font_style);
  textfield->SetAccessibleName(label_text);

  layout->AddView(label);
  layout->AddView(textfield);

  return textfield;
}

views::Textfield* AddTextfieldRow(GridLayout* layout,
                                  const base::string16& label,
                                  int column_set_id) {
  layout->AddPaddingRow(kFixed, ChromeLayoutProvider::Get()->GetDistanceMetric(
                                    DISTANCE_CONTROL_LIST_VERTICAL));
  return AddFirstTextfieldRow(layout, label, column_set_id);
}
