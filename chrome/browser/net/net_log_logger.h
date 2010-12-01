// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
#define CHROME_BROWSER_NET_NET_LOG_LOGGER_H_
#pragma once

#include "chrome/browser/net/chrome_net_log.h"

// NetLogLogger watches the NetLog event stream, and sends all entries to
// VLOG(1).  This is to debug errors that prevent getting to the
// about:net-internals page.
class NetLogLogger : public ChromeNetLog::ThreadSafeObserver {
 public:
  NetLogLogger();
  ~NetLogLogger();

  // ThreadSafeObserver implementation:
  virtual void OnAddEntry(net::NetLog::EventType type,
                          const base::TimeTicks& time,
                          const net::NetLog::Source& source,
                          net::NetLog::EventPhase phase,
                          net::NetLog::EventParameters* params);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetLogLogger);
};

#endif  // CHROME_BROWSER_NET_NET_LOG_LOGGER_H_

