// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NET_LOG_H_
#define CHROME_BROWSER_NET_CHROME_NET_LOG_H_
#pragma once

#include "base/atomicops.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "net/base/net_log.h"

class LoadTimingObserver;
class NetLogLogger;

// ChromeNetLog is an implementation of NetLog that dispatches network log
// messages to a list of observers.
//
// All methods are thread safe, with the exception that no ChromeNetLog or
// ChromeNetLog::ThreadSafeObserverImpl functions may be called by an observer's
// OnAddEntry() method.  Doing so will result in a deadlock.
class ChromeNetLog : public net::NetLog {
 public:
  // Base class for observing the events logged by the network
  // stack. This has some nice-to-have functionality for use by code
  // within chrome/, but any net::NetLog::ThreadSafeObserver may be
  // registered to observe the NetLog.
  //
  // This class uses composition rather than inheritance so that
  // certain invariants can be maintained when subclasses of it are
  // added as observers (through the AddAsObserver and
  // RemoveAsObserver methods on this class).
  class ThreadSafeObserverImpl {
   public:
    // Observers that need to see the full granularity of events can
    // specify LOG_ALL.
    explicit ThreadSafeObserverImpl(LogLevel log_level);
    virtual ~ThreadSafeObserverImpl();

    virtual void OnAddEntry(EventType type,
                            const base::TimeTicks& time,
                            const Source& source,
                            EventPhase phase,
                            EventParameters* params) = 0;

    // These must be used instead of
    // ChromeNetLog::Add/RemoveThreadSafeObserver to manage the
    // association in this class with a given ChromeNetLog instance.
    void AddAsObserver(ChromeNetLog* net_log);
    void RemoveAsObserver();

    void SetLogLevel(LogLevel log_level);

   protected:
    void AssertNetLogLockAcquired() const;

   private:
    class PassThroughObserver : public ThreadSafeObserver {
     public:
      PassThroughObserver(ThreadSafeObserverImpl* owner, LogLevel log_level);
      virtual ~PassThroughObserver() {}
      virtual void OnAddEntry(EventType type,
                              const base::TimeTicks& time,
                              const Source& source,
                              EventPhase phase,
                              EventParameters* params) OVERRIDE;

      // Can only be called when actively observing a ChromeNetLog.
      void SetLogLevel(LogLevel log_level);

     private:
      ThreadSafeObserverImpl* owner_;
    };

    friend class PassThroughObserver;

    // ChromeNetLog currently being observed. Set by
    // AddAsObserver/RemoveAsObserver.
    ChromeNetLog* net_log_;

    // The observer we register in AddAsObserver, that passes stuff
    // through to us.
    PassThroughObserver internal_observer_;

    DISALLOW_COPY_AND_ASSIGN(ThreadSafeObserverImpl);
  };

  ChromeNetLog();
  virtual ~ChromeNetLog();

  // NetLog implementation:
  virtual void AddEntry(EventType type,
                        const base::TimeTicks& time,
                        const Source& source,
                        EventPhase phase,
                        EventParameters* params) OVERRIDE;
  virtual uint32 NextID() OVERRIDE;
  virtual LogLevel GetLogLevel() const OVERRIDE;

  LoadTimingObserver* load_timing_observer() {
    return load_timing_observer_.get();
  }

 private:
  // NetLog implementation
  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;
  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;

  // Called whenever an observer is added or removed, or changes its log level.
  // Must have acquired |lock_| prior to calling.
  void UpdateLogLevel();

  // |lock_| protects access to |observers_| and, indirectly, to. Should not
  // be acquired by observers.
  base::Lock lock_;

  // Last assigned source ID.  Incremented to get the next one.
  base::subtle::Atomic32 last_id_;

  // The lowest allowed log level, regardless of any ChromeNetLogObservers.
  // Normally defaults to LOG_BASIC, but can be changed with command line flags.
  LogLevel base_log_level_;

  // The current log level.
  base::subtle::Atomic32 effective_log_level_;

  scoped_ptr<LoadTimingObserver> load_timing_observer_;
  scoped_ptr<NetLogLogger> net_log_logger_;

  // |lock_| must be acquired whenever reading or writing to this.
  ObserverList<ThreadSafeObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

#endif  // CHROME_BROWSER_NET_CHROME_NET_LOG_H_
