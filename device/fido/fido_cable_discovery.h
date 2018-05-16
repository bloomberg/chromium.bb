// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_CABLE_DISCOVERY_H_
#define DEVICE_FIDO_FIDO_CABLE_DISCOVERY_H_

#include <stdint.h>

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_ble_discovery_base.h"

namespace device {

class BluetoothDevice;
class BluetoothAdvertisement;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoCableDiscovery
    : public FidoBleDiscoveryBase {
 public:
  static constexpr size_t kEphemeralIdSize = 16;
  using EidArray = std::array<uint8_t, kEphemeralIdSize>;

  // Encapsulates information required to discover Cable device per single
  // credential. When multiple credentials are enrolled to a single account
  // (i.e. more than one phone has been enrolled to an user account as a
  // security key), then FidoCableDiscovery must advertise for all of the client
  // EID received from the relying party.
  struct COMPONENT_EXPORT(DEVICE_FIDO) CableDiscoveryData {
    CableDiscoveryData(const EidArray& client_eid,
                       const EidArray& authenticator_eid);
    CableDiscoveryData(const CableDiscoveryData& data);
    CableDiscoveryData& operator=(const CableDiscoveryData& other);
    ~CableDiscoveryData();

    EidArray client_eid;
    EidArray authenticator_eid;
  };

  FidoCableDiscovery(std::vector<CableDiscoveryData> discovery_data);
  ~FidoCableDiscovery() override;

 private:
  // BluetoothAdapter::Observer:
  void DeviceAdded(BluetoothAdapter* adapter, BluetoothDevice* device) override;
  void DeviceChanged(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;
  void DeviceRemoved(BluetoothAdapter* adapter,
                     BluetoothDevice* device) override;

  // FidoBleDiscoveryBase:
  void OnSetPowered() override;

  void StartAdvertisement(const EidArray& client_eid);
  void OnAdvertisementRegistered(
      const EidArray& client_eid,
      scoped_refptr<BluetoothAdvertisement> advertisement);
  void OnAdvertisementRegisterError(
      BluetoothAdvertisement::ErrorCode error_code);
  void CableDeviceFound(BluetoothAdapter* adapter, BluetoothDevice* device);
  bool IsExpectedCaBleDevice(const BluetoothDevice* device) const;

  std::vector<CableDiscoveryData> discovery_data_;
  std::map<EidArray, scoped_refptr<BluetoothAdvertisement>> advertisements_;
  base::WeakPtrFactory<FidoCableDiscovery> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FidoCableDiscovery);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_CABLE_DISCOVERY_H_
