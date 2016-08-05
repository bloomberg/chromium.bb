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

  // Entries are created in WAITING state. At some point they will be PROCESSING
  // into one of the three state: PROCESSED, the only state a distilled URL
  // would be set, WILL_RETRY, similar to wait, but with exponential delays or
  // ERROR where the system will not retry at all.
  enum DistillationState { WAITING, PROCESSING, PROCESSED, WILL_RETRY, ERROR };

  // The URL of the page the user would like to read later.
  const GURL& URL() const;
  // The title of the entry. Might be empty.
  const std::string Title() const;
  // What state this entry is in.
  DistillationState DistilledState() const;
  // The local file URL for the distilled version of the page. This should only
  // be called if the state is "PROCESSED".
  const GURL& DistilledURL() const;

  ReadingListEntry& operator=(const ReadingListEntry& other);
  bool operator==(const ReadingListEntry& other) const;

  // Sets the title.
  void SetTitle(const std::string& title);
  // Sets the distilled URL and switch the state to PROCESSED.
  void SetDistilledURL(const GURL& url);
  // Sets the state to one of PROCESSING, WILL_RETRY or ERROR.
  void SetDistilledState(DistillationState distilled_state);

  // DEPRECATED.
  const GURL& url() const;
  const std::string title() const;

 private:
  GURL url_;
  std::string title_;
  GURL distilled_url_;
  DistillationState distilled_state_;
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_
