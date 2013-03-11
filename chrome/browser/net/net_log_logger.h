// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
#define CHROME_BROWSER_NET_NET_LOG_LOGGER_H_

#include "base/memory/scoped_handle.h"
#include "net/base/net_log.h"

namespace base {
class FilePath;
}

// NetLogLogger watches the NetLog event stream, and sends all entries to
// VLOG(1) or a path specified on creation.  This is to debug errors that
// prevent getting to the about:net-internals page.
//
// When writing directly to a file rather than VLOG(1), the text file will
// contain a single JSON object, with an extra comma on the end and missing
// a terminal "]}".
//
// Relies on ChromeNetLog only calling an Observer once at a time for
// thread-safety.
class NetLogLogger : public net::NetLog::ThreadSafeObserver {
 public:
  // If |log_path| is empty or file creation fails, writes to VLOG(1).
  // Otherwise, writes to |log_path|.  Uses one line per entry, for
  // easy parsing.
  explicit NetLogLogger(const base::FilePath &log_path);
  virtual ~NetLogLogger();

  // Starts observing specified NetLog.  Must not already be watching a NetLog.
  // Separate from constructor to enforce thread safety.
  void StartObserving(net::NetLog* net_log);

  // Stops observing net_log().  Must already be watching.
  void StopObserving();

  // net::NetLog::ThreadSafeObserver implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

 private:
  ScopedStdioHandle file_;

  // True if OnAddEntry() has been called at least once.
  bool added_events_;

  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

#endif  // CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
