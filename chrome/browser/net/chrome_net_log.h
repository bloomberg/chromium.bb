// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CHROME_NET_LOG_H_
#define CHROME_BROWSER_NET_CHROME_NET_LOG_H_

#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "net/base/net_log.h"

class LoadTimingObserver;
class PassiveLogCollector;

// ChromeNetLog is an implementation of NetLog that dispatches network log
// messages to a list of observers.
//
// By default, ChromeNetLog will attach the observer PassiveLogCollector which
// will keep track of recent request information (which used when displaying
// the about:net-internals page).
//
// TODO(eroman): Move this default observer out of ChromeNetLog.
//
class ChromeNetLog : public net::NetLog {
 public:
  // Interface for observing the events logged by the network stack.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnAddEntry(EventType type,
                            const base::TimeTicks& time,
                            const Source& source,
                            EventPhase phase,
                            EventParameters* params) = 0;
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
  virtual bool HasListener() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  PassiveLogCollector* passive_collector() {
    return passive_collector_.get();
  }

  LoadTimingObserver* load_timing_observer() {
    return load_timing_observer_.get();
  }

 private:
  uint32 next_id_;
  scoped_ptr<PassiveLogCollector> passive_collector_;
  scoped_ptr<LoadTimingObserver> load_timing_observer_;
  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNetLog);
};

#endif  // CHROME_BROWSER_NET_CHROME_NET_LOG_H_
