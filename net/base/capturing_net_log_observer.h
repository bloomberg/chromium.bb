// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CAPTURING_NET_LOG_OBSERVER_H_
#define NET_BASE_CAPTURING_NET_LOG_OBSERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "net/base/captured_net_log_entry.h"
#include "net/base/net_log.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace net {

// CapturingNetLogObserver is an implementation of NetLog::ThreadSafeObserver
// that saves messages to a bounded buffer. It is intended for testing only,
// and is part of the net_test_support project.
class CapturingNetLogObserver : public NetLog::ThreadSafeObserver {
 public:
  CapturingNetLogObserver();
  ~CapturingNetLogObserver() override;

  // Returns the list of all entries in the log.
  void GetEntries(CapturedNetLogEntry::List* entry_list) const;

  // Fills |entry_list| with all entries in the log from the specified Source.
  void GetEntriesForSource(NetLog::Source source,
                           CapturedNetLogEntry::List* entry_list) const;

  // Returns number of entries in the log.
  size_t GetSize() const;

  void Clear();

 private:
  // ThreadSafeObserver implementation:
  void OnAddEntry(const NetLog::Entry& entry) override;

  // Needs to be "mutable" so can use it in GetEntries().
  mutable base::Lock lock_;

  CapturedNetLogEntry::List captured_entries_;

  DISALLOW_COPY_AND_ASSIGN(CapturingNetLogObserver);
};

}  // namespace net

#endif  // NET_BASE_CAPTURING_NET_LOG_OBSERVER_H_
