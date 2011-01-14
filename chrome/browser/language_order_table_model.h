// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_ORDER_TABLE_MODEL_H_
#define CHROME_BROWSER_LANGUAGE_ORDER_TABLE_MODEL_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/models/table_model.h"

namespace ui {
class TableModelObserver;
}

class LanguageOrderTableModel : public ui::TableModel {
 public:
  LanguageOrderTableModel();

  virtual ~LanguageOrderTableModel();

  // Set Language List.
  void SetAcceptLanguagesString(const std::string& language_list);

  // Add at the end.  Return true if the language was added.
  bool Add(const std::string& language);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Returns index corresponding to a given language. Returns -1 if the
  // language is not found.
  int GetIndex(const std::string& language);

  // Move down the entry at the specified index.
  void MoveDown(int index);

  // Move up the entry at the specified index.
  void MoveUp(int index);

  // Returns the set of languagess this model contains.
  std::string GetLanguageList();

  // TableModel overrides:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column_id) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

 private:
  // Set of entries we're showing.
  std::vector<std::string> languages_;
  std::string comma_separated_language_list_;

  ui::TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(LanguageOrderTableModel);
};

#endif  // CHROME_BROWSER_LANGUAGE_ORDER_TABLE_MODEL_H_
