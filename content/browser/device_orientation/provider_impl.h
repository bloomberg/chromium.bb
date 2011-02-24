// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_IMPL_H_

#include <set>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"

class MessageLoop;

namespace base {
class Thread;
}

namespace device_orientation {

class ProviderImpl : public Provider {
 public:
  typedef DataFetcher* (*DataFetcherFactory)();

  // Create a ProviderImpl that uses the NULL-terminated factories array to find
  // a DataFetcher that can provide orientation data.
  ProviderImpl(const DataFetcherFactory factories[]);

  // From Provider.
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);

 private:
  virtual ~ProviderImpl();

  // Starts or Stops the provider. Called from creator_loop_.
  void Start();
  void Stop();

  // Method for finding a suitable DataFetcher and starting the polling.
  // Runs on the polling_thread_.
  void DoInitializePollingThread(std::vector<DataFetcherFactory> factories);
  void ScheduleInitializePollingThread();

  // Method for polling a DataFetcher. Runs on the polling_thread_.
  void DoPoll();
  void ScheduleDoPoll();

  // Method for notifying observers of an orientation update.
  // Runs on the creator_thread_.
  void DoNotify(const Orientation& orientation);
  void ScheduleDoNotify(const Orientation& orientation);

  static bool SignificantlyDifferent(const Orientation& orientation1,
                                     const Orientation& orientation2);

  enum { kDesiredSamplingIntervalMs = 100 };
  int SamplingIntervalMs() const;

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  MessageLoop* creator_loop_;

  // Members below are only to be used from the creator_loop_.
  std::vector<DataFetcherFactory> factories_;
  std::set<Observer*> observers_;
  Orientation last_notification_;

  // When polling_thread_ is running, members below are only to be used
  // from that thread.
  scoped_ptr<DataFetcher> data_fetcher_;
  Orientation last_orientation_;
  ScopedRunnableMethodFactory<ProviderImpl> do_poll_method_factory_;

  // Polling is done on this background thread.
  scoped_ptr<base::Thread> polling_thread_;
};

}  // namespace device_orientation

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_IMPL_H_
