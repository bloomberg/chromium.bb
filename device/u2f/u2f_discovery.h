// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_DISCOVERY_H_
#define DEVICE_U2F_U2F_DISCOVERY_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"

namespace device {

class U2fDevice;

class U2fDiscovery {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void DiscoveryStarted(U2fDiscovery* discovery, bool success) = 0;
    virtual void DiscoveryStopped(U2fDiscovery* discovery, bool success) = 0;
    virtual void DeviceAdded(U2fDiscovery* discovery, U2fDevice* device) = 0;
    virtual void DeviceRemoved(U2fDiscovery* discovery, U2fDevice* device) = 0;
  };

  U2fDiscovery();
  virtual ~U2fDiscovery();

  virtual void Start() = 0;
  virtual void Stop() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyDiscoveryStarted(bool success);
  void NotifyDiscoveryStopped(bool success);
  void NotifyDeviceAdded(U2fDevice* device);
  void NotifyDeviceRemoved(U2fDevice* device);

  std::vector<U2fDevice*> GetDevices();
  std::vector<const U2fDevice*> GetDevices() const;

  U2fDevice* GetDevice(base::StringPiece device_id);
  const U2fDevice* GetDevice(base::StringPiece device_id) const;

 protected:
  virtual bool AddDevice(std::unique_ptr<U2fDevice> device);
  virtual bool RemoveDevice(base::StringPiece device_id);

  std::map<std::string, std::unique_ptr<U2fDevice>, std::less<>> devices_;
  base::ObserverList<Observer> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(U2fDiscovery);
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_DISCOVERY_H_
