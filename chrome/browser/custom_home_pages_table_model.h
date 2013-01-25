// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HOME_PAGES_TABLE_MODEL_H_
#define CHROME_BROWSER_CUSTOM_HOME_PAGES_TABLE_MODEL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/history/history_service.h"
#include "ui/base/models/table_model.h"

class GURL;
class Profile;

namespace ui {
class TableModelObserver;
}

// CustomHomePagesTableModel is the model for the TableView showing the list
// of pages the user wants opened on startup.

class CustomHomePagesTableModel : public ui::TableModel {
 public:
  explicit CustomHomePagesTableModel(Profile* profile);
  virtual ~CustomHomePagesTableModel();

  // Sets the set of urls that this model contains.
  void SetURLs(const std::vector<GURL>& urls);

  // Collect all entries indexed by |index_list|, and moves them to be right
  // before the element addressed by |insert_before|. Used by Drag&Drop.
  // Expects |index_list| to be ordered ascending.
  void MoveURLs(int insert_before, const std::vector<int>& index_list);

  // Adds an entry at the specified index.
  void Add(int index, const GURL& url);

  // Removes the entry at the specified index.
  void Remove(int index);

  // Clears any entries and fills the list with pages currently opened in the
  // browser.
  void SetToCurrentlyOpenPages();

  // Returns the set of urls this model contains.
  std::vector<GURL> GetURLs();

  // TableModel overrides:
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int column_id) OVERRIDE;
  virtual string16 GetTooltip(int row) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

 private:
  // Each item in the model is represented as an Entry. Entry stores the URL
  // and title of the page.
  struct Entry;

  // Loads the title for the specified entry.
  void LoadTitle(Entry* entry);

  // Callback from history service. Updates the title of the Entry whose
  // |title_handle| matches |handle| and notifies the observer of the change.
  void OnGotTitle(HistoryService::Handle handle,
                  bool found_url,
                  const history::URLRow* row,
                  history::VisitVector* visits);

  // Returns the entry whose |member| matches |handle| and sets |entry_index| to
  // the index of the entry.
  Entry* GetEntryByLoadHandle(CancelableRequestProvider::Handle Entry::* member,
                              CancelableRequestProvider::Handle handle,
                              int* entry_index);

  // Returns the URL for a particular row, formatted for display to the user.
  string16 FormattedURL(int row) const;

  // Set of entries we're showing.
  std::vector<Entry> entries_;

  // Profile used to load titles.
  Profile* profile_;

  ui::TableModelObserver* observer_;

  // Used in loading titles.
  CancelableRequestConsumer history_query_consumer_;

  DISALLOW_COPY_AND_ASSIGN(CustomHomePagesTableModel);
};

#endif  // CHROME_BROWSER_CUSTOM_HOME_PAGES_TABLE_MODEL_H_
