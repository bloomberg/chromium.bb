// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
#define CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
#pragma once

#include "base/scoped_handle.h"
#include "chrome/browser/net/chrome_net_log.h"

class FilePath;

// NetLogLogger watches the NetLog event stream, and sends all entries to
// VLOG(1) or a path specified on creation.  This is to debug errors that
// prevent getting to the about:net-internals page.
//
// Relies on ChromeNetLog only calling an Observer once at a time for
// thread-safety.
class NetLogLogger : public ChromeNetLog::ThreadSafeObserver {
 public:
  // If |log_path| is empty or file creation fails, writes to VLOG(1).
  // Otherwise, writes to |log_path|.  Uses one line per entry, for
  // easy parsing.
  explicit NetLogLogger(const FilePath &log_path);
  ~NetLogLogger();

  // ThreadSafeObserver implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);

 private:
  ScopedStdioHandle file_;

  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

#endif  // CHROME_BROWSER_NET_NET_LOG_LOGGER_H_

