// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include "chrome/browser/webdata/autofill_entry.h"

bool AutofillKey::operator==(const AutofillKey& key) const {
  return name_ == key.name() && value_ == key.value();
}

bool AutofillEntry::operator==(const AutofillEntry& entry) const {
  if (!(key_ == entry.key()))
    return false;

  if (timestamps_.size() != entry.timestamps().size())
    return false;

  std::set<base::Time> other_timestamps(entry.timestamps().begin(),
                                        entry.timestamps().end());
  for (size_t i = 0; i < timestamps_.size(); i++) {
    if (other_timestamps.count(timestamps_[i]) == 0)
      return false;
  }

  return true;
}
