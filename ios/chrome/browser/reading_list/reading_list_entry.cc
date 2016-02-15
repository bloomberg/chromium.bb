// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

ReadingListEntry::ReadingListEntry(const GURL& url, const std::string& title)
    : url_(url), title_(title) {
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());
}
ReadingListEntry::ReadingListEntry(const ReadingListEntry& entry)
    : url_(entry.url()), title_(entry.title()) {}
ReadingListEntry::~ReadingListEntry() {}

const GURL& ReadingListEntry::url() const {
  return url_;
}

const std::string ReadingListEntry::title() const {
  return title_;
}

ReadingListEntry& ReadingListEntry::operator=(const ReadingListEntry& other) {
  url_ = other.url_;
  title_ = other.title_;
  return *this;
}

bool ReadingListEntry::operator==(const ReadingListEntry& other) const {
  return url_ == other.url_;
}
