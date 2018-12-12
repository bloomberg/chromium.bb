// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_

#include <vector>

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

namespace send_tab_to_self {

// The send tab to self model contains a list of entries of shared urls.
// This object should only be accessed from one thread, which is usually the
// main thread.
class SendTabToSelfModel {
 public:
  SendTabToSelfModel();
  virtual ~SendTabToSelfModel();

  // returns true if the model is finished loading, sent tabs are not accessible
  // while this is false.
  virtual bool loaded() const = 0;

  // Returns a vector of URLs in the model. The order of the URL is not
  // specified and can vary on successive calls.
  virtual const std::vector<GURL> Keys() const = 0;

  // Returns the total number of entries in the model.
  virtual size_t size() const = 0;

  // Delete all entries. Return true if entries where indeed deleted.
  virtual bool DeleteAllEntries() = 0;

  // Returns a specific entry. Returns null if the entry does not exist.
  virtual const SendTabToSelfEntry* GetEntryByURL(const GURL& gurl) const = 0;

  // Returns the most recently shared entry.
  virtual const SendTabToSelfEntry* GetMostRecentEntry() const = 0;

  // Adds |url| at the top of the entries The entry title will be a
  // trimmed copy of |title|.
  // The addition may be asynchronous, and the data will be available only once
  // the observers are notified.
  virtual const SendTabToSelfEntry* AddEntry(const GURL& url,
                                             const std::string& title) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModel);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H
