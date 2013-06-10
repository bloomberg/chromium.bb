// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_IMPORTED_BOOKMARK_ENTRY_H_
#define CHROME_BROWSER_BOOKMARKS_IMPORTED_BOOKMARK_ENTRY_H_

#include <vector>

#include "base/strings/string16.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"

struct ImportedBookmarkEntry {
  ImportedBookmarkEntry();
  ~ImportedBookmarkEntry();

  bool operator==(const ImportedBookmarkEntry& other) const;

  bool in_toolbar;
  bool is_folder;
  GURL url;
  std::vector<base::string16> path;
  base::string16 title;
  base::Time creation_time;
};

#endif  // CHROME_BROWSER_BOOKMARKS_IMPORTED_BOOKMARK_ENTRY_H_
