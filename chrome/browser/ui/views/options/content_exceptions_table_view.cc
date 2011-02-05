// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/content_exceptions_table_view.h"

#include "ui/gfx/font.h"

ContentExceptionsTableView::ContentExceptionsTableView(
    ContentExceptionsTableModel* model,
    const std::vector<TableColumn>& columns)
    : views::TableView(model, columns, views::TEXT_ONLY, false, true, false),
      exceptions_(model) {
  SetCustomColorsEnabled(true);
}

bool ContentExceptionsTableView::GetCellColors(int model_row,
                                               int column,
                                               ItemColor* foreground,
                                               ItemColor* background,
                                               LOGFONT* logfont) {
  if (!exceptions_->entry_is_off_the_record(model_row))
    return false;

  foreground->color_is_set = false;
  background->color_is_set = false;

  gfx::Font font;
  font = font.DeriveFont(0, gfx::Font::ITALIC);
  HFONT hf = font.GetNativeFont();
  GetObject(hf, sizeof(LOGFONT), logfont);
  return true;
}
