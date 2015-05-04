// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_TEST_NET_LOG_OBSERVER_H_
#define NET_LOG_TEST_NET_LOG_OBSERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "net/log/net_log.h"
#include "net/log/test_net_log_entry.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace net {

// TestNetLogObserver is an implementation of NetLog::ThreadSafeObserver
// that saves messages to a bounded buffer. It is intended for testing only,
// and is part of the net_test_support project.
class TestNetLogObserver : public NetLog::ThreadSafeObserver {
 public:
  TestNetLogObserver();
  ~TestNetLogObserver() override;

  // Returns the list of all entries in the log.
  void GetEntries(TestNetLogEntry::List* entry_list) const;

  // Fills |entry_list| with all entries in the log from the specified Source.
  void GetEntriesForSource(NetLog::Source source,
                           TestNetLogEntry::List* entry_list) const;

  // Returns number of entries in the log.
  size_t GetSize() const;

  void Clear();

 private:
  // ThreadSafeObserver implementation:
  void OnAddEntry(const NetLog::Entry& entry) override;

  // Needs to be "mutable" so can use it in GetEntries().
  mutable base::Lock lock_;

  TestNetLogEntry::List entry_list_;

  DISALLOW_COPY_AND_ASSIGN(TestNetLogObserver);
};

}  // namespace net

#endif  // NET_LOG_TEST_NET_LOG_OBSERVER_H_
