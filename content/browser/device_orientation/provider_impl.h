// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/device_data.h"
#include "content/browser/device_orientation/provider.h"
#include "content/common/content_export.h"

class MessageLoop;

namespace content {

class ProviderImpl : public Provider {
 public:
  typedef DataFetcher* (*DataFetcherFactory)();

  // Create a ProviderImpl that uses the factory to create a DataFetcher that
  // can provide data. A NULL DataFetcherFactory indicates that there are no
  // DataFetchers for this OS.
  CONTENT_EXPORT ProviderImpl(DataFetcherFactory factory);

  // From Provider.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;

 private:
  class PollingThread;

  virtual ~ProviderImpl();

  // Starts or Stops the provider. Called from creator_loop_.
  void Start(DeviceData::Type type);
  void Stop();

  void ScheduleInitializePollingThread(DeviceData::Type device_data_type);
  void ScheduleDoAddPollingDataType(DeviceData::Type type);

  // Method for notifying observers of a data update.
  // Runs on the creator_thread_.
  void DoNotify(const scoped_refptr<const DeviceData>& data,
                DeviceData::Type device_data_type);

  static bool ShouldFireEvent(const DeviceData* old_data,
      const DeviceData* new_data, DeviceData::Type device_data_type);

  // The Message Loop on which this object was created.
  // Typically the I/O loop, but may be something else during testing.
  MessageLoop* creator_loop_;

  // Members below are only to be used from the creator_loop_.
  DataFetcherFactory factory_;
  std::set<Observer*> observers_;
  std::map<DeviceData::Type, scoped_refptr<const DeviceData> >
      last_notifications_map_;

  // When polling_thread_ is running, members below are only to be used
  // from that thread.
  base::WeakPtrFactory<ProviderImpl> weak_factory_;

  // Polling is done on this background thread. PollingThread is owned by
  // the ProviderImpl object. But its deletion doesn't happen synchronously
  // along with deletion of the ProviderImpl. Thus this should be a raw
  // pointer instead of scoped_ptr.
  PollingThread* polling_thread_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_PROVIDER_IMPL_H_
