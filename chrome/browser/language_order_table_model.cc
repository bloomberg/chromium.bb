// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language_order_table_model.h"

#include <set>

#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/table_model_observer.h"

LanguageOrderTableModel::LanguageOrderTableModel()
    : observer_(NULL) {
}

LanguageOrderTableModel::~LanguageOrderTableModel() {}

void LanguageOrderTableModel::SetAcceptLanguagesString(
    const std::string& language_list) {
  std::vector<std::string> languages_vector;
  base::SplitString(language_list, ',', &languages_vector);
  languages_.clear();
  std::set<std::string> added;
  for (int i = 0; i < static_cast<int>(languages_vector.size()); i++) {
    const std::string& language(languages_vector.at(i));
    if (!language.empty() && added.count(language) == 0) {
      languages_.push_back(language);
      added.insert(language);
    }
  }
  if (observer_)
    observer_->OnModelChanged();
}

void LanguageOrderTableModel::SetObserver(ui::TableModelObserver* observer) {
  observer_ = observer;
}

string16 LanguageOrderTableModel::GetText(int row, int column_id) {
  DCHECK(row >= 0 && row < RowCount());
  const std::string app_locale = g_browser_process->GetApplicationLocale();
  return l10n_util::GetDisplayNameForLocale(languages_.at(row),
                                            app_locale,
                                            true);
}

bool LanguageOrderTableModel::Add(const std::string& language) {
  if (language.empty())
    return false;
  // Check for selecting duplicated language.
  for (std::vector<std::string>::const_iterator cit = languages_.begin();
       cit != languages_.end(); ++cit)
    if (*cit == language)
      return false;
  languages_.push_back(language);
  if (observer_)
    observer_->OnItemsAdded(RowCount() - 1, 1);
  return true;
}

void LanguageOrderTableModel::Remove(int index) {
  DCHECK(index >= 0 && index < RowCount());
  languages_.erase(languages_.begin() + index);
  if (observer_)
    observer_->OnItemsRemoved(index, 1);
}

int LanguageOrderTableModel::GetIndex(const std::string& language) {
  if (language.empty())
    return -1;

  int index = 0;
  for (std::vector<std::string>::const_iterator cit = languages_.begin();
      cit != languages_.end(); ++cit) {
    if (*cit == language)
      return index;

    index++;
  }

  return -1;
}

void LanguageOrderTableModel::MoveDown(int index) {
  if (index < 0 || index >= RowCount() - 1)
    return;
  std::string item = languages_.at(index);
  languages_.erase(languages_.begin() + index);
  if (index == RowCount() - 1)
    languages_.push_back(item);
  else
    languages_.insert(languages_.begin() + index + 1, item);
  if (observer_)
    observer_->OnItemsChanged(0, RowCount());
}

void LanguageOrderTableModel::MoveUp(int index) {
  if (index <= 0 || index >= static_cast<int>(languages_.size()))
    return;
  std::string item = languages_.at(index);
  languages_.erase(languages_.begin() + index);
  languages_.insert(languages_.begin() + index - 1, item);
  if (observer_)
    observer_->OnItemsChanged(0, RowCount());
}

std::string LanguageOrderTableModel::GetLanguageList() {
  return JoinString(languages_, ',');
}

int LanguageOrderTableModel::RowCount() {
  return static_cast<int>(languages_.size());
}
