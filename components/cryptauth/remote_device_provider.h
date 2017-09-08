// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_H_
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_H_

#include "base/observer_list.h"
#include "components/cryptauth/remote_device.h"

namespace cryptauth {

// This class generates and caches RemoteDevice objects when associated metadata
// has been synced, and updates this cache when a new sync occurs.
class RemoteDeviceProvider {
 public:
  class Observer {
   public:
    virtual void OnSyncDeviceListChanged() {}

   protected:
    virtual ~Observer() {}
  };

  RemoteDeviceProvider();

  virtual ~RemoteDeviceProvider();

  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual void NotifyObserversDeviceListChanged();

  // Returns a list of all RemoteDevices that have been synced.
  virtual const cryptauth::RemoteDeviceList GetSyncedDevices() const = 0;

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_PROVIDER_H_
