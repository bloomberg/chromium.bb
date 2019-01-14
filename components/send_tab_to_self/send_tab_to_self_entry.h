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
class SendTabToSelfEntry {
 public:
  // Creates a SendTabToSelf entry. |url| and |title| are the main fields of the
  // entry.
  // |now| is used to fill the |creation_time_us_| and all the update timestamp
  // fields.
  SendTabToSelfEntry(const std::string& guid,
                     const GURL& url,
                     const std::string& title,
                     base::Time shared_time,
                     base::Time original_navigation_time,
                     const std::string& device_name);
  ~SendTabToSelfEntry();

  // The unique random id for the entry.
  const std::string& GetGUID() const;
  // The URL of the page the user would like to send to themselves.
  const GURL& GetURL() const;
  // The title of the entry. Might be empty.
  const std::string& GetTitle() const;
  // The time that the tab was shared.
  base::Time GetSharedTime() const;
  // The time that the tab was navigated to.
  base::Time GetOriginalNavigationTime() const;

  // The name of the device that originated the sent tab.
  const std::string& GetDeviceName() const;

  // Returns a protobuf encoding the content of this SendTabToSelfEntry for
  // sync.
  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> AsProto() const;

  // Creates a SendTabToSelfEntry from the protobuf format.
  // If creation time is not set, it will be set to |now|.
  static std::unique_ptr<SendTabToSelfEntry> FromProto(
      const sync_pb::SendTabToSelfSpecifics& pb_entry,
      base::Time now);

 private:
  std::string guid_;
  GURL url_;
  std::string title_;
  std::string device_name_;
  base::Time shared_time_;
  base::Time original_navigation_time_;

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfEntry);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_ENTRY_H_
