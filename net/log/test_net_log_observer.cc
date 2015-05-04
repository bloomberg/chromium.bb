// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/test_net_log_observer.h"

#include "base/values.h"

namespace net {

TestNetLogObserver::TestNetLogObserver() {
}

TestNetLogObserver::~TestNetLogObserver() {
}

void TestNetLogObserver::GetEntries(TestNetLogEntry::List* entry_list) const {
  base::AutoLock lock(lock_);
  *entry_list = entry_list_;
}

void TestNetLogObserver::GetEntriesForSource(
    NetLog::Source source,
    TestNetLogEntry::List* entry_list) const {
  base::AutoLock lock(lock_);
  entry_list->clear();
  for (TestNetLogEntry::List::const_iterator entry = entry_list_.begin();
       entry != entry_list_.end(); ++entry) {
    if (entry->source.id == source.id)
      entry_list->push_back(*entry);
  }
}

size_t TestNetLogObserver::GetSize() const {
  base::AutoLock lock(lock_);
  return entry_list_.size();
}

void TestNetLogObserver::Clear() {
  base::AutoLock lock(lock_);
  entry_list_.clear();
}

void TestNetLogObserver::OnAddEntry(const NetLog::Entry& entry) {
  // Using Dictionaries instead of Values makes checking values a little
  // simpler.
  base::DictionaryValue* param_dict = nullptr;
  base::Value* param_value = entry.ParametersToValue();
  if (param_value && !param_value->GetAsDictionary(&param_dict))
    delete param_value;

  // Only need to acquire the lock when accessing class variables.
  base::AutoLock lock(lock_);
  entry_list_.push_back(TestNetLogEntry(
      entry.type(), base::TimeTicks::Now(), entry.source(), entry.phase(),
      scoped_ptr<base::DictionaryValue>(param_dict)));
}

}  // namespace net
