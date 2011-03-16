// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
#define CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/search_engines/template_url_model_observer.h"
#include "ui/base/models/table_model.h"

class ModelEntry;
class SkBitmap;
class TemplateURL;
class TemplateURLModel;

// TemplateURLTableModel is the TableModel implementation used by
// KeywordEditorView to show the keywords in a TableView.
//
// TemplateURLTableModel has two columns, the first showing the description,
// the second the keyword.
//
// TemplateURLTableModel maintains a vector of ModelEntrys that correspond to
// each row in the tableview. Each ModelEntry wraps a TemplateURL, providing
// the favicon. The entries in the model are sorted such that non-generated
// appear first (grouped together) and are followed by generated keywords.

class TemplateURLTableModel : public ui::TableModel,
                                     TemplateURLModelObserver {
 public:
  explicit TemplateURLTableModel(TemplateURLModel* template_url_model);

  virtual ~TemplateURLTableModel();

  // Reloads the entries from the TemplateURLModel. This should ONLY be invoked
  // if the TemplateURLModel wasn't initially loaded and has been loaded.
  void Reload();

  // ui::TableModel overrides.
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column) OVERRIDE;
  virtual SkBitmap GetIcon(int row) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;
  virtual bool HasGroups() OVERRIDE;
  virtual Groups GetGroups() OVERRIDE;
  virtual int GetGroupID(int row) OVERRIDE;

  // Removes the entry at the specified index.
  void Remove(int index);

  // Adds a new entry at the specified index.
  void Add(int index, TemplateURL* template_url);

  // Update the entry at the specified index.
  void ModifyTemplateURL(int index,
                         const string16& title,
                         const string16& keyword,
                         const std::string& url);

  // Reloads the icon at the specified index.
  void ReloadIcon(int index);

  // Returns The TemplateURL at the specified index.
  const TemplateURL& GetTemplateURL(int index);

  // Returns the index of the TemplateURL, or -1 if it the TemplateURL is not
  // found.
  int IndexOfTemplateURL(const TemplateURL* template_url);

  // Moves the keyword at the specified index to be at the end of the main
  // group. Returns the new index.  If the entry is already in the main group,
  // no action is taken, and the current index is returned.
  int MoveToMainGroup(int index);

  // Make the TemplateURL at |index| the default.  Returns the new index, or -1
  // if the index is invalid or it is already the default.
  int MakeDefaultTemplateURL(int index);

  // If there is an observer, it's notified the selected row has changed.
  void NotifyChanged(int index);

  TemplateURLModel* template_url_model() const { return template_url_model_; }

  // Returns the index of the last entry shown in the search engines group.
  int last_search_engine_index() const { return last_search_engine_index_; }

 private:
  friend class ModelEntry;

  // Notification that a model entry has fetched its icon.
  void FaviconAvailable(ModelEntry* entry);

  // TemplateURLModelObserver notification.
  virtual void OnTemplateURLModelChanged();

  ui::TableModelObserver* observer_;

  // The entries.
  std::vector<ModelEntry*> entries_;

  // The model we're displaying entries from.
  TemplateURLModel* template_url_model_;

  // Index of the last search engine in entries_. This is used to determine the
  // group boundaries.
  int last_search_engine_index_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLTableModel);
};


#endif  // CHROME_BROWSER_UI_SEARCH_ENGINES_TEMPLATE_URL_TABLE_MODEL_H_
