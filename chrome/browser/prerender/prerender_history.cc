// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_history.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"

namespace prerender {

PrerenderHistory::PrerenderHistory(size_t max_items)
    : max_items_(max_items) {
  DCHECK(max_items > 0);
}

PrerenderHistory::~PrerenderHistory() {
}

void PrerenderHistory::AddEntry(const Entry& entry) {
  DCHECK(CalledOnValidThread());
  while (entries_.size() >= max_items_)
    entries_.pop_front();
  entries_.push_back(entry);
}

void PrerenderHistory::Clear() {
  entries_.clear();
}

base::Value* PrerenderHistory::GetEntriesAsValue() const {
  base::ListValue* return_list = new base::ListValue();
  // Javascript needs times in terms of milliseconds since Jan 1, 1970.
  base::Time epoch_start = base::Time::UnixEpoch();
  for (std::list<Entry>::const_reverse_iterator it = entries_.rbegin();
       it != entries_.rend();
       ++it) {
    const Entry& entry = *it;
    base::DictionaryValue* entry_dict = new base::DictionaryValue();
    entry_dict->SetString("url", entry.url.spec());
    entry_dict->SetString("final_status",
                          NameFromFinalStatus(entry.final_status));
    entry_dict->SetString("origin", NameFromOrigin(entry.origin));
    // Use a string to prevent overflow, as Values don't support 64-bit
    // integers.
    entry_dict->SetString(
        "end_time",
        base::Int64ToString((entry.end_time - epoch_start).InMilliseconds()));
    return_list->Append(entry_dict);
  }
  return return_list;
}

}  // namespace prerender
