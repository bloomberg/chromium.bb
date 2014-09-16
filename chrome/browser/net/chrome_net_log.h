// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NET_LOG_H_
#define CHROME_BROWSER_NET_CHROME_NET_LOG_H_

#include "base/atomicops.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "net/base/net_log.h"

namespace net {
class NetLogLogger;
class TraceNetLogObserver;
}

class NetLogTempFile;

// ChromeNetLog is an implementation of NetLog that adds file loggers
// as its observers.
class ChromeNetLog : public net::NetLog {
 public:
  ChromeNetLog();
  virtual ~ChromeNetLog();

  NetLogTempFile* net_log_temp_file() {
    return net_log_temp_file_.get();
  }

 private:
  scoped_ptr<net::NetLogLogger> net_log_logger_;
  scoped_ptr<NetLogTempFile> net_log_temp_file_;

  scoped_ptr<net::TraceNetLogObserver> trace_net_log_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

#endif  // CHROME_BROWSER_NET_CHROME_NET_LOG_H_
