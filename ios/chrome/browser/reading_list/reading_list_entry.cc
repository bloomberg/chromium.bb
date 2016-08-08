// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

ReadingListEntry::ReadingListEntry(const GURL& url, const std::string& title)
    : url_(url), title_(title), distilled_state_(WAITING) {
  DCHECK(!url.is_empty());
  DCHECK(url.is_valid());
}
ReadingListEntry::ReadingListEntry(const ReadingListEntry& entry)
    : url_(entry.URL()),
      title_(entry.Title()),
      distilled_url_(entry.DistilledURL()),
      distilled_state_(entry.DistilledState()) {}
ReadingListEntry::~ReadingListEntry() {}

const GURL& ReadingListEntry::URL() const {
  return url_;
}

const std::string ReadingListEntry::Title() const {
  return title_;
}

ReadingListEntry::DistillationState ReadingListEntry::DistilledState() const {
  return distilled_state_;
}

const GURL& ReadingListEntry::DistilledURL() const {
  return distilled_url_;
}

ReadingListEntry& ReadingListEntry::operator=(const ReadingListEntry& other) {
  url_ = other.url_;
  title_ = other.title_;
  distilled_url_ = other.distilled_url_;
  distilled_state_ = other.distilled_state_;
  return *this;
}

bool ReadingListEntry::operator==(const ReadingListEntry& other) const {
  return url_ == other.url_;
}

void ReadingListEntry::SetTitle(const std::string& title) {
  title_ = title;
}

void ReadingListEntry::SetDistilledURL(const GURL& url) {
  DCHECK(url.is_valid());
  distilled_url_ = url;
  distilled_state_ = PROCESSED;
}

void ReadingListEntry::SetDistilledState(DistillationState distilled_state) {
  DCHECK(distilled_state != PROCESSED);  // use SetDistilledURL instead.
  DCHECK(distilled_state != WAITING);
  distilled_state_ = distilled_state;
  distilled_url_ = GURL();
}
