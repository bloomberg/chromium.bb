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
// ChromeNetLog::ThreadSafeObserverImpl functions may be called by an observer's
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
    // specify LOG_ALL. However doing so will have performance consequences,
    // and may cause PassiveLogCollector to use more memory than anticipated.
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

    void AddAsObserverAndGetAllPassivelyCapturedEvents(
        ChromeNetLog *net_log,
        EntryList* passive_entries);

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

  void GetAllPassivelyCapturedEvents(EntryList* passive_entries);

  void ClearAllPassivelyCapturedEvents();

  LoadTimingObserver* load_timing_observer() {
    return load_timing_observer_.get();
  }

 private:
  void AddObserverWhileLockHeld(ThreadSafeObserver* observer);

  // NetLog implementation
  virtual void AddThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;
  virtual void RemoveThreadSafeObserver(ThreadSafeObserver* observer) OVERRIDE;

  // Adds |observer| and writes all passively captured events to
  // |passive_entries|. Guarantees that no events in |passive_entries| will be
  // sent to |observer| and all future events that have yet been sent to the
  // PassiveLogCollector will be sent to |observer|.
  void AddObserverAndGetAllPassivelyCapturedEvents(ThreadSafeObserver* observer,
                                                   EntryList* passive_entries);


  // Called whenever an observer is added or removed, or changes its log level.
  // Must have acquired |lock_| prior to calling.
  void UpdateLogLevel();

  // |lock_| protects access to |observers_| and, indirectly, to
  // |passive_collector_|.  Should not be acquired by observers.
  base::Lock lock_;

  // Last assigned source ID.  Incremented to get the next one.
  base::subtle::Atomic32 last_id_;

  // The lowest allowed log level, regardless of any ChromeNetLogObservers.
  // Normally defaults to LOG_BASIC, but can be changed with command line flags.
  LogLevel base_log_level_;

  // The current log level.
  base::subtle::Atomic32 effective_log_level_;

  // Not thread safe.  Must only be used when |lock_| is acquired.
  scoped_ptr<PassiveLogCollector> passive_collector_;

  scoped_ptr<LoadTimingObserver> load_timing_observer_;
  scoped_ptr<NetLogLogger> net_log_logger_;

  // |lock_| must be acquired whenever reading or writing to this.
  ObserverList<ThreadSafeObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

#endif  // CHROME_BROWSER_NET_CHROME_NET_LOG_H_
