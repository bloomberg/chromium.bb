// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NET_LOG_H_
#define CHROME_BROWSER_NET_CHROME_NET_LOG_H_
#pragma once

#include <vector>

#include "base/atomicops.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "net/base/net_log.h"

class LoadTimingObserver;
class NetLogLogger;
class PassiveLogCollector;

// ChromeNetLog is an implementation of NetLog that dispatches network log
// messages to a list of observers.
//
// All methods are thread safe, with the exception that no ChromeNetLog or
// ChromeNetLog::ThreadSafeObserver functions may be called by an observer's
// OnAddEntry() method.  Doing so will result in a deadlock.
//
// By default, ChromeNetLog will attach the observer PassiveLogCollector which
// will keep track of recent request information (which used when displaying
// the about:net-internals page).
//
class ChromeNetLog : public net::NetLog {
 public:
  // This structure encapsulates all of the parameters of an event,
  // including an "order" field that identifies when it was captured relative
  // to other events.
  struct Entry {
    Entry(uint32 order,
          net::NetLog::EventType type,
          const base::TimeTicks& time,
          net::NetLog::Source source,
          net::NetLog::EventPhase phase,
          net::NetLog::EventParameters* params);
    ~Entry();

    uint32 order;
    net::NetLog::EventType type;
    base::TimeTicks time;
    net::NetLog::Source source;
    net::NetLog::EventPhase phase;
    scoped_refptr<net::NetLog::EventParameters> params;
  };

  typedef std::vector<Entry> EntryList;

  // Interface for observing the events logged by the network stack.
  class ThreadSafeObserver {
   public:
    // Constructs an observer that wants to see network events, with
    // the specified minimum event granularity.  A ThreadSafeObserver can only
    // observe a single ChromeNetLog at a time.
    //
    // Typical observers should specify LOG_BASIC.
    //
    // Observers that need to see the full granularity of events can
    // specify LOG_ALL. However doing so will have performance consequences,
    // and may cause PassiveLogCollector to use more memory than anticipated.
    //
    // Observers will be called on the same thread an entry is added on,
    // and are responsible for ensuring their own thread safety.
    explicit ThreadSafeObserver(LogLevel log_level);

    virtual ~ThreadSafeObserver();

    // This method will be called on the thread that the event occurs on.  It
    // is the responsibility of the observer to handle it in a thread safe
    // manner.
    //
    // It is illegal for an Observer to call any ChromeNetLog or
    // ChromeNetLog::ThreadSafeObserver functions in response to a call to
    // OnAddEntry.
    virtual void OnAddEntry(EventType type,
                            const base::TimeTicks& time,
                            const Source& source,
                            EventPhase phase,
                            EventParameters* params) = 0;
    LogLevel log_level() const;

   protected:
    void AssertNetLogLockAcquired() const;

    // Can only be called when actively observing a ChromeNetLog.
    void SetLogLevel(LogLevel log_level);

    // ChromeNetLog currently being observed, if any.  Set by ChromeNetLog's
    // AddObserver and RemoveObserver methods.
    ChromeNetLog* net_log_;

   private:
    friend class ChromeNetLog;
    LogLevel log_level_;
    DISALLOW_COPY_AND_ASSIGN(ThreadSafeObserver);
  };

  ChromeNetLog();
  ~ChromeNetLog();

  // NetLog implementation:
  virtual void AddEntry(EventType type,
                        const base::TimeTicks& time,
                        const Source& source,
                        EventPhase phase,
                        EventParameters* params);
  virtual uint32 NextID();
  virtual LogLevel GetLogLevel() const;

  void AddObserver(ThreadSafeObserver* observer);
  void RemoveObserver(ThreadSafeObserver* observer);

  // Adds |observer| and writes all passively captured events to
  // |passive_entries|. Guarantees that no events in |passive_entries| will be
  // sent to |observer| and all future events that have yet been sent to the
  // PassiveLogCollector will be sent to |observer|.
  void AddObserverAndGetAllPassivelyCapturedEvents(ThreadSafeObserver* observer,
                                                   EntryList* passive_entries);

  void GetAllPassivelyCapturedEvents(EntryList* passive_entries);

  void ClearAllPassivelyCapturedEvents();

  LoadTimingObserver* load_timing_observer() {
    return load_timing_observer_.get();
  }

 private:
  void AddObserverWhileLockHeld(ThreadSafeObserver* observer);

  // Called whenever an observer is added or removed, or changes its log level.
  // Must have acquired |lock_| prior to calling.
  void UpdateLogLevel_();

  // |lock_| protects access to |observers_| and, indirectly, to
  // |passive_collector_|.  Should not be acquired by observers.
  base::Lock lock_;

  // Last assigned source ID.  Incremented to get the next one.
  base::subtle::Atomic32 last_id_;

  base::subtle::Atomic32 log_level_;

  // Not thread safe.  Must only be used when |lock_| is acquired.
  scoped_ptr<PassiveLogCollector> passive_collector_;

  scoped_ptr<LoadTimingObserver> load_timing_observer_;
  scoped_ptr<NetLogLogger> net_log_logger_;

  // |lock_| must be acquired whenever reading or writing to this.
  ObserverList<ThreadSafeObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

#endif  // CHROME_BROWSER_NET_CHROME_NET_LOG_H_
