// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_READING_LIST_IOS_READING_LIST_ENTRY_H_
#define COMPONENTS_READING_LIST_IOS_READING_LIST_ENTRY_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace reading_list {
class ReadingListLocal;
}

namespace sync_pb {
class ReadingListSpecifics;
}

class ReadingListEntry;

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
  // The local file path for the distilled version of the page. This should only
  // be called if the state is "PROCESSED".
  const base::FilePath& DistilledPath() const;
  // The time before the next try. This is automatically increased when the
  // state is set to WILL_RETRY or ERROR from a non-error state.
  base::TimeDelta TimeUntilNextTry() const;
  // The number of time chrome failed to download this entry. This is
  // automatically increased when the state is set to WILL_RETRY or ERROR from a
  // non-error state.
  int FailedDownloadCounter() const;
  // The read status of the entry.
  bool IsRead() const;

  // The last update time of the entry. This value may be used to sort the
  // entries. The value is in microseconds since Jan 1st 1970.
  int64_t UpdateTime() const;

  // The creation update time of the entry. The value is in microseconds since
  // Jan 1st 1970.
  int64_t CreationTime() const;

  // Set the update time to now.
  void MarkEntryUpdated();

  // Returns a protobuf encoding the content of this ReadingListEntry for local
  // storage.
  std::unique_ptr<reading_list::ReadingListLocal> AsReadingListLocal() const;

  // Returns a protobuf encoding the content of this ReadingListEntry for sync.
  std::unique_ptr<sync_pb::ReadingListSpecifics> AsReadingListSpecifics() const;

  // Created a ReadingListEntry from the protobuf format.
  static std::unique_ptr<ReadingListEntry> FromReadingListLocal(
      const reading_list::ReadingListLocal& pb_entry);

  // Created a ReadingListEntry from the protobuf format.
  static std::unique_ptr<ReadingListEntry> FromReadingListSpecifics(
      const sync_pb::ReadingListSpecifics& pb_entry);

  // Merge the local data from |other| to this.
  // The local fields (distilled_state_, distilled_url_, backoff_,
  // failed_download_counter_) of |other| are moved to |this| and must not be
  // used after this call.
  void MergeLocalStateFrom(ReadingListEntry& other);

  ReadingListEntry& operator=(ReadingListEntry&& other);

  bool operator==(const ReadingListEntry& other) const;

  // Returns whether |lhs| is more recent than |rhs|.
  static bool CompareEntryUpdateTime(const ReadingListEntry& lhs,
                                     const ReadingListEntry& rhs);

  // Sets the title.
  void SetTitle(const std::string& title);
  // Sets the distilled URL and switch the state to PROCESSED and reset the time
  // until the next try.
  void SetDistilledPath(const base::FilePath& path);
  // Sets the state to one of PROCESSING, WILL_RETRY or ERROR.
  void SetDistilledState(DistillationState distilled_state);
  // Sets the read stat of the entry. Will set the UpdateTime of the entry.
  void SetRead(bool read);

 private:
  ReadingListEntry(const GURL& url,
                   const std::string& title,
                   bool read,
                   int64_t creation_time,
                   int64_t update_time,
                   ReadingListEntry::DistillationState distilled_state,
                   const base::FilePath& distilled_path,
                   int failed_download_counter,
                   std::unique_ptr<net::BackoffEntry> backoff);
  GURL url_;
  std::string title_;
  bool read_;
  base::FilePath distilled_path_;
  DistillationState distilled_state_;

  std::unique_ptr<net::BackoffEntry> backoff_;
  int failed_download_counter_;

  // These value are in microseconds since Jan 1st 1970. They are used for
  // sorting the entries from the database. They are kept in int64_t to avoid
  // conversion on each save/read event.
  int64_t creation_time_us_;
  int64_t update_time_us_;

  DISALLOW_COPY_AND_ASSIGN(ReadingListEntry);
};

#endif  // COMPONENTS_READING_LIST_IOS_READING_LIST_ENTRY_H_
