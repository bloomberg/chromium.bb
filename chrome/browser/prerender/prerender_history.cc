// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_history.h"

#include "base/logging.h"
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

Value* PrerenderHistory::GetEntriesAsValue() const {
  ListValue* return_list = new ListValue();
  for (std::list<Entry>::const_reverse_iterator it = entries_.rbegin();
       it != entries_.rend();
       ++it) {
    const Entry& entry = *it;
    DictionaryValue* v = new DictionaryValue();
    v->SetString("url", entry.url.spec());
    v->SetString("final_status", NameFromFinalStatus(entry.final_status));
    return_list->Append(v);
  }
  return return_list;
}

}  // namespace prerender
