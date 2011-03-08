// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_EXCEPTIONS_TABLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_EXCEPTIONS_TABLE_VIEW_H_
#pragma once

#include "chrome/browser/content_exceptions_table_model.h"
#include "views/controls/table/table_view.h"

// A thin wrapper around TableView that displays incognito entries in
// italics.
class ContentExceptionsTableView : public views::TableView {
 public:
  ContentExceptionsTableView(ContentExceptionsTableModel* model,
                             const std::vector<TableColumn>& columns);

  virtual ~ContentExceptionsTableView() {}

 private:
  virtual bool GetCellColors(int model_row,
                             int column,
                             ItemColor* foreground,
                             ItemColor* background,
                             LOGFONT* logfont);

  ContentExceptionsTableModel* exceptions_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_EXCEPTIONS_TABLE_VIEW_H_
