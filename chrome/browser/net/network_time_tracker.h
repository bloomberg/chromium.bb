// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NETWORK_TIME_TRACKER_H_
#define CHROME_BROWSER_NET_NETWORK_TIME_TRACKER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time.h"
#include "net/base/network_time_notifier.h"

class IOThread;
class NetworkTimeTrackerTest;

// A class that receives network time updates and can provide the network time
// for a corresponding local time. This class is not thread safe, but may live
// on any thread.
// Note that all NetworkTimeTracker instances interact with a
// NetworkTimeNotifier singleton that lives on the IO thread. This class
// enables arbitrary threads to maintain their own network time source without
// having to deal with the details of the Notifier's implementation.
class NetworkTimeTracker {
 public:
  // Callback for updating network time based on http responses. This callback
  // is thread safe and should be called directly without posting to any thread.
  // The parameters are:
  // const base::Time& network_time - the actual time.
  // const base::TimeDelta& resolution - how precise the reading is.
  // const base::TimeDelta& latency - the http request's latency.
  typedef base::Callback<void(const base::Time&,
                              const base::TimeDelta&,
                              const base::TimeDelta&)> UpdateCallback;

  NetworkTimeTracker();
  ~NetworkTimeTracker();

  // Starts tracking network time.
  void Start();

  // Returns the network time corresponding to |time_ticks| if network time
  // is available. Returns false if no network time is available yet. Can also
  // return the error range if |uncertainty| isn't NULL.
  bool GetNetworkTime(const base::TimeTicks& time_ticks,
                      base::Time* network_time,
                      base::TimeDelta* uncertainty) const;

  // Build a callback for use in updating the network time notifier.
  // This method must be called on the UI thread, while the callback can be
  // called from any thread.
  static UpdateCallback BuildNotifierUpdateCallback();

 private:
  friend class NetworkTimeTrackerTest;

  // Returns the callback used to register for time updates from the network
  // time notifier on the IO thread.
  net::NetworkTimeNotifier::ObserverCallback BuildObserverCallback();

  // net::NetworkTimeNotifier::ObserverCallback implementation.
  void OnNetworkTimeUpdate(
      const base::Time& network_time,
      const base::TimeTicks& network_time_ticks,
      const base::TimeDelta& network_time_uncertainty);

  // Network time based on last call to UpdateNetworkTime().
  base::Time network_time_;

  // The estimated ticks count from when |network_time_| was set based on the
  // system's tick count.
  base::TimeTicks network_time_ticks_;

  // Uncertainty of |network_time_| based on added inaccuracies/resolution.
  base::TimeDelta network_time_uncertainty_;

  base::WeakPtrFactory<NetworkTimeTracker> weak_ptr_factory_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTimeTracker);
};

#endif  // CHROME_BROWSER_NET_NETWORK_TIME_TRACKER_H_
