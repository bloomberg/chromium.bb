// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {

class FidoDevice;

namespace internal {
class ScopedFidoDiscoveryFactory;
}

class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscovery {
 public:
  class COMPONENT_EXPORT(DEVICE_FIDO) Observer {
   public:
    virtual ~Observer();
    virtual void DiscoveryStarted(FidoDiscovery* discovery, bool success) = 0;
    virtual void DiscoveryStopped(FidoDiscovery* discovery, bool success) = 0;
    virtual void DeviceAdded(FidoDiscovery* discovery, FidoDevice* device) = 0;
    virtual void DeviceRemoved(FidoDiscovery* discovery,
                               FidoDevice* device) = 0;
  };

  // Factory function to construct an instance that discovers authenticators on
  // the given |transport| protocol.
  //
  // U2fTransportProtocol::kUsbHumanInterfaceDevice requires specifying a valid
  // |connector| on Desktop, and is not valid on Android.
  static std::unique_ptr<FidoDiscovery> Create(
      U2fTransportProtocol transport,
      ::service_manager::Connector* connector);

  virtual ~FidoDiscovery();

  virtual U2fTransportProtocol GetTransportProtocol() const = 0;
  virtual void Start() = 0;
  virtual void Stop() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyDiscoveryStarted(bool success);
  void NotifyDiscoveryStopped(bool success);
  void NotifyDeviceAdded(FidoDevice* device);
  void NotifyDeviceRemoved(FidoDevice* device);

  std::vector<FidoDevice*> GetDevices();
  std::vector<const FidoDevice*> GetDevices() const;

  FidoDevice* GetDevice(base::StringPiece device_id);
  const FidoDevice* GetDevice(base::StringPiece device_id) const;

 protected:
  FidoDiscovery();

  virtual bool AddDevice(std::unique_ptr<FidoDevice> device);
  virtual bool RemoveDevice(base::StringPiece device_id);

  std::map<std::string, std::unique_ptr<FidoDevice>, std::less<>> devices_;
  base::ObserverList<Observer> observers_;

 private:
  friend class internal::ScopedFidoDiscoveryFactory;

  // Factory function can be overridden by tests to construct fakes.
  using FactoryFuncPtr = decltype(&Create);
  static FactoryFuncPtr g_factory_func_;

  DISALLOW_COPY_AND_ASSIGN(FidoDiscovery);
};

namespace internal {

// Base class for a scoped override of FidoDiscovery::Create, used in unit
// tests, layout tests, and when running with the Web Authn Testing API enabled.
//
// While there is a subclass instance in scope, calls to the factory method will
// be hijacked such that the derived class's CreateFidoDiscovery method will be
// invoked instead.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedFidoDiscoveryFactory {
 public:
  ScopedFidoDiscoveryFactory();
  virtual ~ScopedFidoDiscoveryFactory();

 protected:
  virtual std::unique_ptr<FidoDiscovery> CreateFidoDiscovery(
      U2fTransportProtocol transport,
      ::service_manager::Connector* connector) = 0;

 private:
  static std::unique_ptr<FidoDiscovery>
  ForwardCreateFidoDiscoveryToCurrentFactory(
      U2fTransportProtocol transport,
      ::service_manager::Connector* connector);

  static ScopedFidoDiscoveryFactory* g_current_factory;
  ScopedFidoDiscoveryFactory* original_factory_;
  FidoDiscovery::FactoryFuncPtr original_factory_func_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFidoDiscoveryFactory);
};

}  // namespace internal
}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_H_
