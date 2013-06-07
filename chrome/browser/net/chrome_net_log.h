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

class NetLogLogger;
class NetLogTempFile;

// ChromeNetLog is an implementation of NetLog that dispatches network log
// messages to a list of observers.
//
// All methods are thread safe, with the exception that no NetLog or
// NetLog::ThreadSafeObserver functions may be called by an observer's
// OnAddEntry() method.  Doing so will result in a deadlock.
class ChromeNetLog : public net::NetLog {
 public:
  ChromeNetLog();
  virtual ~ChromeNetLog();

  // NetLog implementation:
  virtual uint32 NextID() OVERRIDE;
  virtual LogLevel GetLogLevel() const OVERRIDE;
  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer,
                                     LogLevel log_level) OVERRIDE;
  virtual void SetObserverLogLevel(ThreadSafeObserver* observer,
                                   LogLevel log_level) OVERRIDE;
  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;

  NetLogTempFile* net_log_temp_file() {
    return net_log_temp_file_.get();
  }

 private:
  // NetLog implementation:
  virtual void OnAddEntry(const net::NetLog::Entry& entry) OVERRIDE;

  // Called whenever an observer is added or removed, or has its log level
  // changed.  Must have acquired |lock_| prior to calling.
  void UpdateLogLevel();

  // |lock_| protects access to |observers_|.
  base::Lock lock_;

  // Last assigned source ID.  Incremented to get the next one.
  base::subtle::Atomic32 last_id_;

  // The lowest allowed log level, regardless of any ChromeNetLogObservers.
  // Normally defaults to LOG_BASIC, but can be changed with command line flags.
  LogLevel base_log_level_;

  // The current log level.
  base::subtle::Atomic32 effective_log_level_;

  scoped_ptr<NetLogLogger> net_log_logger_;
  scoped_ptr<NetLogTempFile> net_log_temp_file_;

  // |lock_| must be acquired whenever reading or writing to this.
  ObserverList<ThreadSafeObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

#endif  // CHROME_BROWSER_NET_CHROME_NET_LOG_H_
