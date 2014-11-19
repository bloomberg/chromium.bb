// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CAPTURING_NET_LOG_H_
#define NET_BASE_CAPTURING_NET_LOG_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "net/base/captured_net_log_entry.h"
#include "net/base/capturing_net_log_observer.h"
#include "net/base/net_log.h"

namespace net {

// CapturingNetLog is convenience class which combines a NetLog and a
// CapturingNetLogObserver.  It is intended for testing only, and is part of the
// net_test_support project.
class CapturingNetLog : public NetLog {
 public:
  // TODO(mmenke):  Get rid of these.
  typedef CapturedNetLogEntry CapturedEntry;
  typedef CapturedNetLogEntry::List CapturedEntryList;

  CapturingNetLog();
  ~CapturingNetLog() override;

  void SetLogLevel(LogLevel log_level);

  // Below methods are forwarded to capturing_net_log_observer_.
  void GetEntries(CapturedEntryList* entry_list) const;
  void GetEntriesForSource(Source source, CapturedEntryList* entry_list) const;
  size_t GetSize() const;
  void Clear();

 private:
  CapturingNetLogObserver capturing_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(CapturingNetLog);
};

// Helper class that exposes a similar API as BoundNetLog, but uses a
// CapturingNetLog rather than the more generic NetLog.
//
// A CapturingBoundNetLog can easily be converted to a BoundNetLog using the
// bound() method.
class CapturingBoundNetLog {
 public:
  CapturingBoundNetLog();
  ~CapturingBoundNetLog();

  // The returned BoundNetLog is only valid while |this| is alive.
  BoundNetLog bound() const { return net_log_; }

  // Fills |entry_list| with all entries in the log.
  void GetEntries(CapturingNetLog::CapturedEntryList* entry_list) const;

  // Fills |entry_list| with all entries in the log from the specified Source.
  void GetEntriesForSource(
      NetLog::Source source,
      CapturingNetLog::CapturedEntryList* entry_list) const;

  // Returns number of entries in the log.
  size_t GetSize() const;

  void Clear();

  // Sets the log level of the underlying CapturingNetLog.
  void SetLogLevel(NetLog::LogLevel log_level);

 private:
  CapturingNetLog capturing_net_log_;
  const BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(CapturingBoundNetLog);
};

}  // namespace net

#endif  // NET_BASE_CAPTURING_NET_LOG_H_
