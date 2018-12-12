// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_

#include <string>

#include "base/macros.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace sync_pb {
class SendTabToSelfSpecifics;
}

namespace send_tab_to_self {

// A tab that is being shared. The URL is a unique identifier for an entry, as
// such it should not be empty and is the only thing considered when comparing
// entries.
// A word about timestamp usage in this class:
// - The backing store uses int64 values to code timestamps. We use internally
//   the same type to avoid useless conversions. These values represent the
//   number of micro seconds since Jan 1st 1970.
class SendTabToSelfEntry {
 public:
  // Creates a SendTabToSelf entry. |url| and |title| are the main fields of the
  // entry.
  // |now| is used to fill the |creation_time_us_| and all the update timestamp
  // fields.
  SendTabToSelfEntry(const GURL& url,
                     const std::string& title,
                     base::Time shared_time);
  ~SendTabToSelfEntry();

  // The URL of the page the user would like to send to themselves.
  const GURL& GetURL() const;
  // The title of the entry. Might be empty.
  const std::string& GetTitle() const;
  // The time that the tab was shared. The value is in microseconds since Jan
  // 1st 1970.
  base::Time GetSharedTime() const;

  // Returns a protobuf encoding the content of this SendTabToSelfEntry for
  // sync.
  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> AsProto() const;

  // Creates a SendTabToSelfEntry from the protobuf format.
  // If creation time is not set, it will be set to |now|.
  static std::unique_ptr<SendTabToSelfEntry> FromProto(
      const sync_pb::SendTabToSelfSpecifics& pb_entry,
      base::Time now);

 private:
  SendTabToSelfEntry(const std::string& title,
                     const GURL& url,
                     int64_t shared_time);
  std::string title_;
  GURL url_;

  // These value are in microseconds since Jan 1st 1970. They are used for
  // sorting the entries from the database. They are kept in int64_t to avoid
  // conversion on each save/read event.
  int64_t shared_time_us_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfEntry);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
