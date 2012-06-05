// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TABLE_ROW_NSIMAGE_CACHE_H_
#define CHROME_BROWSER_UI_COCOA_TABLE_ROW_NSIMAGE_CACHE_H_
#pragma once

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"

class SkBitmap;

// There are several dialogs that display tabular data with one SkBitmap
// per row. This class converts these SkBitmaps to NSImages on demand, and
// caches the results.
class TableRowNSImageCache {
 public:
  // Interface this cache expects for its table model.
  class Table {
   public:
    // Returns the number of rows in the table.
    virtual int RowCount() const = 0;

    // Returns the icon of the |row|th row.
    virtual SkBitmap GetIcon(int row) const = 0;

   protected:
    virtual ~Table() {}
  };

  // |model| must outlive the cache.
  explicit TableRowNSImageCache(Table* model);
  ~TableRowNSImageCache();

  // Lazily converts the image at the given row and caches it in |icon_images_|.
  NSImage* GetImageForRow(int row);

  // Call these functions every time the table changes, to update the cache.
  void OnModelChanged();
  void OnItemsChanged(int start, int length);
  void OnItemsAdded(int start, int length);
  void OnItemsRemoved(int start, int length);

 private:
  // The table model we query for row count and icons.
  Table* model_;  // weak

  // Stores strong NSImage refs for icons. If an entry is NULL, it will be
  // created in GetImageForRow().
  scoped_nsobject<NSPointerArray> icon_images_;
};

#endif  // CHROME_BROWSER_UI_COCOA_TABLE_ROW_NSIMAGE_CACHE_H_

