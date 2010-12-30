// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POSSIBLE_URL_MODEL_H_
#define CHROME_BROWSER_POSSIBLE_URL_MODEL_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "app/table_model.h"
#include "base/compiler_specific.h"
#include "chrome/browser/history/history.h"

class SkBitmap;

////////////////////////////////////////////////////////////////////////////////
//
// A table model to represent the list of URLs that the user might want to
// bookmark.
//
////////////////////////////////////////////////////////////////////////////////

class PossibleURLModel : public TableModel {
 public:
  PossibleURLModel();
  virtual ~PossibleURLModel();

  void Reload(Profile *profile);

  void OnHistoryQueryComplete(HistoryService::Handle h,
                              history::QueryResults* result);

  const GURL& GetURL(int row);
  const std::wstring& GetTitle(int row);

  virtual void OnFavIconAvailable(FaviconService::Handle h,
                                  bool fav_icon_available,
                                  scoped_refptr<RefCountedMemory> data,
                                  bool expired,
                                  GURL icon_url);

  // TableModel overrides
  virtual int RowCount() OVERRIDE;
  virtual string16 GetText(int row, int col_id) OVERRIDE;
  virtual SkBitmap GetIcon(int row) OVERRIDE;
  virtual int CompareValues(int row1, int row2, int column_id) OVERRIDE;
  virtual void SetObserver(TableModelObserver* observer) OVERRIDE;

 private:
  // The current profile.
  Profile* profile_;

  // Our observer.
  TableModelObserver* observer_;

  // Our consumer for favicon requests.
  CancelableRequestConsumerT<size_t, NULL> consumer_;

  // The results we're showing.
  struct Result;
  std::vector<Result> results_;

  // Map Result::index -> Favicon.
  typedef std::map<size_t, SkBitmap> FavIconMap;
  FavIconMap fav_icon_map_;

  DISALLOW_COPY_AND_ASSIGN(PossibleURLModel);
};

#endif  // CHROME_BROWSER_POSSIBLE_URL_MODEL_H_
