// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_ORDER_TABLE_MODEL_H_
#define CHROME_BROWSER_LANGUAGE_ORDER_TABLE_MODEL_H_
#pragma once

#include <string>
#include <vector>

#include "app/table_model.h"
#include "base/basictypes.h"

class TableModelObserver;

class LanguageOrderTableModel : public TableModel {
 public:
  LanguageOrderTableModel();

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
  std::string GetLanguageList() { return VectorToList(languages_); }

  // TableModel overrides:
  virtual int RowCount();
  virtual std::wstring GetText(int row, int column_id);
  virtual void SetObserver(TableModelObserver* observer);

 private:
  // This method converts a comma separated list to a vector of strings.
  void ListToVector(const std::string& list,
                    std::vector<std::string>* vector);

  // This method returns a comma separated string given a string vector.
  std::string VectorToList(const std::vector<std::string>& vector);

  // Set of entries we're showing.
  std::vector<std::string> languages_;
  std::string comma_separated_language_list_;

  TableModelObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(LanguageOrderTableModel);
};

#endif  // CHROME_BROWSER_LANGUAGE_ORDER_TABLE_MODEL_H_
