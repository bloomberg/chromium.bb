// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CUSTOM_HOME_PAGES_TABLE_MODEL_H_
#define CHROME_BROWSER_CUSTOM_HOME_PAGES_TABLE_MODEL_H_
#pragma once

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/favicon_service.h"
#include "ui/base/models/table_model.h"

class GURL;
class Profile;
class SkBitmap;

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
  virtual SkBitmap GetIcon(int row) OVERRIDE;
  virtual string16 GetTooltip(int row) OVERRIDE;
  virtual void SetObserver(ui::TableModelObserver* observer) OVERRIDE;

 private:
  // Each item in the model is represented as an Entry. Entry stores the URL,
  // title, and favicon of the page.
  struct Entry;

  // Loads the title and favicon for the specified entry.
  void LoadTitleAndFavIcon(Entry* entry);

  // Callback from history service. Updates the title of the Entry whose
  // |title_handle| matches |handle| and notifies the observer of the change.
  void OnGotTitle(HistoryService::Handle handle,
                  bool found_url,
                  const history::URLRow* row,
                  history::VisitVector* visits);

  // Callback from history service. Updates the icon of the Entry whose
  // |favicon_handle| matches |handle| and notifies the observer of the change.
  void OnGotFavIcon(FaviconService::Handle handle,
                    history::FaviconData favicon);

  // Returns the entry whose |member| matches |handle| and sets |entry_index| to
  // the index of the entry.
  Entry* GetEntryByLoadHandle(CancelableRequestProvider::Handle Entry::* member,
                              CancelableRequestProvider::Handle handle,
                              int* entry_index);

  // Returns the entry whose |favicon_handle| matches |handle| and sets
  // |entry_index| to the index of the entry.
  Entry* GetEntryByFavIconHandle(FaviconService::Handle handle,
                                 int* entry_index);

  // Returns the URL for a particular row, formatted for display to the user.
  string16 FormattedURL(int row) const;

  // Set of entries we're showing.
  std::vector<Entry> entries_;

  // Default icon to show when one can't be found for the URL.
  SkBitmap* default_favicon_;

  // Profile used to load titles and icons.
  Profile* profile_;

  ui::TableModelObserver* observer_;

  // Used in loading titles and favicons.
  CancelableRequestConsumer query_consumer_;

  DISALLOW_COPY_AND_ASSIGN(CustomHomePagesTableModel);
};

#endif  // CHROME_BROWSER_CUSTOM_HOME_PAGES_TABLE_MODEL_H_
