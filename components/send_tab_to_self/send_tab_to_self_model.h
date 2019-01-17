// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H_

#include <vector>

#include "base/observer_list.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace send_tab_to_self {

// The send tab to self model contains a list of entries of shared urls.
// This object should only be accessed from one thread, which is usually the
// main thread.
class SendTabToSelfModel {
 public:
  SendTabToSelfModel();
  virtual ~SendTabToSelfModel();

  // Returns a vector of entry IDs in the model.
  virtual std::vector<std::string> GetAllGuids() const = 0;

  // Delete all entries.
  virtual void DeleteAllEntries() = 0;

  // Returns a specific entry. Returns null if the entry does not exist.
  virtual const SendTabToSelfEntry* GetEntryByGUID(
      const std::string& guid) const = 0;

  // Adds |url| at the top of the entries. The entry title will be a
  // trimmed copy of |title|.
  virtual const SendTabToSelfEntry* AddEntry(const GURL& url,
                                             const std::string& title,
                                             base::Time navigation_time) = 0;

  // Observer registration methods. The model will remove all observers upon
  // destruction automatically.
  void AddObserver(SendTabToSelfModelObserver* observer);
  void RemoveObserver(SendTabToSelfModelObserver* observer);

 protected:
  // The observers.
  base::ObserverList<SendTabToSelfModelObserver>::Unchecked observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModel);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_H
