// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_
#define IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_

#include <string>

#include "base/macros.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

// An entry in the reading list. The URL is a unique identifier for an entry, as
// such it should not be empty and is the only thing considered when comparing
// entries.
class ReadingListEntry {
 public:
  ReadingListEntry(const GURL& url, const std::string& title);
  ReadingListEntry(const GURL& url,
                   const std::string& title,
                   std::unique_ptr<net::BackoffEntry> backoff);
  ReadingListEntry(ReadingListEntry&& entry);
  ~ReadingListEntry();

  // Entries are created in WAITING state. At some point they will be PROCESSING
  // into one of the three state: PROCESSED, the only state a distilled URL
  // would be set, WILL_RETRY, similar to wait, but with exponential delays or
  // ERROR where the system will not retry at all.
  enum DistillationState { WAITING, PROCESSING, PROCESSED, WILL_RETRY, ERROR };

  static const net::BackoffEntry::Policy kBackoffPolicy;

  // The URL of the page the user would like to read later.
  const GURL& URL() const;
  // The title of the entry. Might be empty.
  const std::string& Title() const;
  // What state this entry is in.
  DistillationState DistilledState() const;
  // The local file URL for the distilled version of the page. This should only
  // be called if the state is "PROCESSED".
  const GURL& DistilledURL() const;
  // The time before the next try. This is automatically increased when the
  // state is set to WILL_RETRY or ERROR from a non-error state.
  base::TimeDelta TimeUntilNextTry() const;
  // The number of time chrome failed to download this entry. This is
  // automatically increased when the state is set to WILL_RETRY or ERROR from a
  // non-error state.
  int FailedDownloadCounter() const;

  ReadingListEntry& operator=(ReadingListEntry&& other);
  bool operator==(const ReadingListEntry& other) const;

  // Sets the title.
  void SetTitle(const std::string& title);
  // Sets the distilled URL and switch the state to PROCESSED and reset the time
  // until the next try.
  void SetDistilledURL(const GURL& url);
  // Sets the state to one of PROCESSING, WILL_RETRY or ERROR.
  void SetDistilledState(DistillationState distilled_state);

 private:
  GURL url_;
  std::string title_;
  GURL distilled_url_;
  DistillationState distilled_state_;
  std::unique_ptr<net::BackoffEntry> backoff_;
  int failed_download_counter_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListEntry);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_READING_LIST_ENTRY_H_
