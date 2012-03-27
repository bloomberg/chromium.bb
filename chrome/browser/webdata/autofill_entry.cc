// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "chrome/browser/webdata/autofill_entry.h"
#include "base/utf_string_conversions.h"

AutofillKey::AutofillKey() {}

AutofillKey::AutofillKey(const string16& name, const string16& value)
    : name_(name),
      value_(value) {
}

AutofillKey::AutofillKey(const char* name, const char* value)
    : name_(UTF8ToUTF16(name)),
      value_(UTF8ToUTF16(value)) {
}

AutofillKey::AutofillKey(const AutofillKey& key)
    : name_(key.name()),
      value_(key.value()) {
}

AutofillKey::~AutofillKey() {}

bool AutofillKey::operator==(const AutofillKey& key) const {
  return name_ == key.name() && value_ == key.value();
}

bool AutofillKey::operator<(const AutofillKey& key) const {
  int diff = name_.compare(key.name());
  if (diff < 0) {
    return true;
  } else if (diff == 0) {
    return value_.compare(key.value()) < 0;
  } else {
    return false;
  }
}

AutofillEntry::AutofillEntry(const AutofillKey& key,
                             const std::vector<base::Time>& timestamps)
    : key_(key) {
  timestamps_culled_ = CullTimeStamps(timestamps, &timestamps_);
}

AutofillEntry::~AutofillEntry() {}

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

bool AutofillEntry::operator<(const AutofillEntry& entry) const {
  return key_ < entry.key();
}

// Culls the list of timestamps to the most recent |kMaxAutofillTimeStamps|.
// If sync is enabled, at every browser restart, sync will do a match up of all
// autofill items on the server with all items on the web db. When webdb loads
// all the items in memory(for sync to process. The method is
// |GetAllAutofillEntries|) they will pass through this method for culling. If
// sync finds any of these items were culled it will updates the server AND the
// web db with these new timestamps. However after restart if an autofill item
// exceeds the |kMaxAutofillTimeStamps| it will NOT be written to web db until
// restart. But sync when uploading to the server will only upload this culled
// list. Meaning until restart there will be mis-match in timestamps but
// it should correct itself at startup.
// Also if sync is not enabled this culling would never take place.
bool AutofillEntry::CullTimeStamps(const std::vector<base::Time>& source,
                                       std::vector<base::Time>* result) {
  DCHECK(result);
  DCHECK(&source != result);

  // First copy the source to result.
  result->clear();
  result->insert(result->begin(), source.begin(), source.end());

  if (source.size() <= kMaxAutofillTimeStamps)
    return false;

  VLOG(1) << "Culling timestamps. Current count is : " << source.size();

  // Regular sort does ascending order. So we pass in a comparator function.
  partial_sort(result->begin(), result->begin() + kMaxAutofillTimeStamps,
               result->end(), std::greater<base::Time>());

  result->erase(result->begin() + kMaxAutofillTimeStamps, result->end());

  return true;
}
