// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/capturing_net_log_observer.h"

#include "base/values.h"

namespace net {

CapturingNetLogObserver::CapturingNetLogObserver() {}

CapturingNetLogObserver::~CapturingNetLogObserver() {}

void CapturingNetLogObserver::GetEntries(
    CapturedNetLogEntry::List* entry_list) const {
  base::AutoLock lock(lock_);
  *entry_list = captured_entries_;
}

void CapturingNetLogObserver::GetEntriesForSource(
    NetLog::Source source,
    CapturedNetLogEntry::List* entry_list) const {
  base::AutoLock lock(lock_);
  entry_list->clear();
  for (CapturedNetLogEntry::List::const_iterator entry =
           captured_entries_.begin();
       entry != captured_entries_.end(); ++entry) {
    if (entry->source.id == source.id)
      entry_list->push_back(*entry);
  }
}

size_t CapturingNetLogObserver::GetSize() const {
  base::AutoLock lock(lock_);
  return captured_entries_.size();
}

void CapturingNetLogObserver::Clear() {
  base::AutoLock lock(lock_);
  captured_entries_.clear();
}

void CapturingNetLogObserver::OnAddEntry(const net::NetLog::Entry& entry) {
  // Using Dictionaries instead of Values makes checking values a little
  // simpler.
  base::DictionaryValue* param_dict = nullptr;
  base::Value* param_value = entry.ParametersToValue();
  if (param_value && !param_value->GetAsDictionary(&param_dict))
    delete param_value;

  // Only need to acquire the lock when accessing class variables.
  base::AutoLock lock(lock_);
  captured_entries_.push_back(
      CapturedNetLogEntry(entry.type(),
                          base::TimeTicks::Now(),
                          entry.source(),
                          entry.phase(),
                          scoped_ptr<base::DictionaryValue>(param_dict)));
}

}  // namespace net
