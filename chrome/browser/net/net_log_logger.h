// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
#define CHROME_BROWSER_NET_NET_LOG_LOGGER_H_

#include <stdio.h>

#include "base/memory/scoped_handle.h"
#include "net/base/net_log.h"

namespace base {
class FilePath;
}

// NetLogLogger watches the NetLog event stream, and sends all entries to
// a file specified on creation.  This is to debug errors in cases where
// about:net-internals doesn't work well (Mobile, startup / shutdown errors,
// errors that prevent getting to the about:net-internals).
//
// The text file will contain a single JSON object.
//
// Relies on ChromeNetLog only calling an Observer once at a time for
// thread-safety.
class NetLogLogger : public net::NetLog::ThreadSafeObserver {
 public:
  // Takes ownership of |file| and will write network events to it once logging
  // starts.  |file| must be non-NULL handle and be open for writing.
  explicit NetLogLogger(FILE* file);
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
