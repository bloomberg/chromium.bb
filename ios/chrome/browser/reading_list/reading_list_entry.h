// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_

#include <string>

#include "url/gurl.h"

// An entry in the reading list. The URL is a unique identifier for an entry, as
// such it should not be empty and is the only thing considered when comparing
// entries.
class ReadingListEntry {
 public:
  ReadingListEntry(const GURL& url, const std::string& title);
  ReadingListEntry(const ReadingListEntry& entry);
  ~ReadingListEntry();

  const GURL& url() const;
  const std::string title() const;

  ReadingListEntry& operator=(const ReadingListEntry& other);
  bool operator==(const ReadingListEntry& other) const;

 private:
  GURL url_;
  std::string title_;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_
