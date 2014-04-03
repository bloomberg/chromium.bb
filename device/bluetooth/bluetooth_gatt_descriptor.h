// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_GATT_DESCRIPTOR_H_
#define DEVICE_BLUETOOTH_GATT_DESCRIPTOR_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "device/bluetooth/bluetooth_utils.h"

namespace device {

class BluetoothGattCharacteristic;

// BluetoothGattDescriptor represents a local or remote GATT characteristic
// descriptor. A GATT characteristic descriptor provides further information
// about a characteristic's value. They can be used to describe the
// characteristic's features or to control certain behaviors.
class BluetoothGattDescriptor {
 public:
  // The Bluetooth Specification declares several predefined descriptors that
  // profiles can use. The following are definitions for the list of UUIDs
  // and descriptions of the characteristic descriptors that they represent.
  // Possible values for and further information on each descriptor can be found
  // in Core v4.0, Volume 3, Part G, Section 3.3.3. All of these desciptors are
  // optional and may not be present for a given characteristic.

  // The "Characteristic Extended Properties" descriptor. This defines
  // additional "Characteristic Properties" which cannot fit into the allocated
  // single octet property field of a characteristic. The value is a bit field
  // and the two predefined bits, as per Bluetooth Core Specification v4.0, are:
  //
  //    - Reliable Write: 0x0001
  //    - Writable Auxiliaries: 0x0002
  //
  static const bluetooth_utils::UUID kCharacteristicExtendedPropertiesUuid;

  // The "Characteristic User Description" descriptor defines a UTF-8 string of
  // variable size that is a user textual description of the associated
  // characteristic's value. There can be only one instance of this descriptor
  // per characteristic. This descriptor can be written to if the "Writable
  // Auxiliaries" bit of the Characteristic Properties (via the "Characteristic
  // Extended Properties" descriptor) has been set.
  static const bluetooth_utils::UUID kCharacteristicUserDescriptionUuid;

  // The "Client Characteristic Configuration" descriptor defines how the
  // characteristic may be configured by a specific client. A server-side
  // instance of this descriptor exists for each client that has bonded with
  // the server and the value can be read and written by that client only. As
  // of Core v4.0, this descriptor is used by clients to set up notifications
  // and indications from a characteristic. The value is a bit field and the
  // predefined bits are:
  //
  //    - Default: 0x0000
  //    - Notification: 0x0001
  //    - Indication: 0x0002
  //
  static const bluetooth_utils::UUID kClientCharacteristicConfigurationUuid;

  // The "Server Characteristic Configuration" descriptor defines how the
  // characteristic may be configured for the server. There is one instance
  // of this descriptor for all clients and setting the value of this descriptor
  // affects its configuration for all clients. As of Core v4.0, this descriptor
  // is used to set up the server to broadcast the characteristic value if
  // advertising resources are available. The value is a bit field and the
  // predefined bits are:
  //
  //    - Default: 0x0000
  //    - Broadcast: 0x0001
  //
  static const bluetooth_utils::UUID kServerCharacteristicConfigurationUuid;

  // The "Characteristic Presentation Format" declaration defines the format of
  // the Characteristic Value. The value is composed of 7 octets which are
  // divided into groups that represent different semantic meanings. For a
  // detailed description of how the value of this descriptor should be
  // interpreted, refer to Core v4.0, Volume 3, Part G, Section 3.3.3.5. If more
  // than one declaration of this descriptor exists for a characteristic, then a
  // "Characteristic Aggregate Format" descriptor must also exist for that
  // characteristic.
  static const bluetooth_utils::UUID kCharacteristicPresentationFormatUuid;

  // The "Characteristic Aggregate Format" descriptor defines the format of an
  // aggragated characteristic value. In GATT's underlying protocol, ATT, each
  // attribute is identified by a handle that is unique for the hosting server.
  // Multiple characteristics can share the same instance(s) of a
  // "Characteristic Presentation Format" descriptor. The value of the
  // "Characteristic Aggregate Format" descriptor contains a list of handles
  // that each refer to a "Characteristic Presentation Format" descriptor that
  // is used by that characteristic. Hence, exactly one instance of this
  // descriptor must exist if more than one "Characteristic Presentation Format"
  // descriptors exist for a characteristic.
  //
  // Applications that are using the device::Bluetooth API do not have access to
  // the underlying handles and shouldn't use this descriptor to determine which
  // "Characteristic Presentation Format" desciptors belong to a characteristic.
  // The API will construct a BluetoothGattDescriptor object for each instance
  // of "Characteristic Presentation Format" descriptor per instance of
  // BluetoothGattCharacteristic that represents a remote characteristic.
  // Similarly for local characteristics, implementations DO NOT need to create
  // an instance of BluetoothGattDescriptor for this descriptor as this will be
  // handled by the subsystem.
  static const bluetooth_utils::UUID kCharacteristicAggregateFormatUuid;

  // Interface for observing changes from a BluetoothGattDescriptor.
  // Properties of remote characteristic desciptors are received asynchonously.
  // The Observer interface can be used to be notified when the initial values
  // of a characteristic descriptor are received as well as when successive
  // changes occur during its life cycle.
  class Observer {
   public:
    // Called when the UUID of |descriptor| has changed.
    virtual void UuidChanged(
        BluetoothGattDescriptor* descriptor,
        const bluetooth_utils::UUID& uuid) {}

    // Called when the current value of |descriptor| has changed.
    virtual void ValueChanged(
        BluetoothGattDescriptor* descriptor,
        const std::vector<uint8>& value) {}
  };

  // The ErrorCallback is used by methods to asynchronously report errors.
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  // The ValueCallback is used to return the value of a remote characteristic
  // descriptor upon a read request.
  typedef base::Callback<void(const std::vector<uint8>&)> ValueCallback;

  // Adds and removes observers for events on this GATT characteristic
  // descriptor. If monitoring multiple descriptors, check the |descriptor|
  // parameter of observer methods to determine which characteristic is issuing
  // the event.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Constructs a BluetoothGattDescriptor that can be associated with a local
  // GATT characteristic when the adapter is in the peripheral role. To
  // associate the returned descriptor with a characteristic, add it to a local
  // characteristic by calling BluetoothGattCharacteristic::AddDescriptor.
  //
  // This method constructs a characteristic descriptor with UUID |uuid| and the
  // initial cached value |value|. |value| will be cached and returned for read
  // requests and automatically modified for write requests by default, unless
  // an instance of BluetoothGattService::Delegate has been provided to the
  // associated BluetoothGattService instance, in which case the delegate will
  // handle the read and write requests.
  //
  // Currently, only custom UUIDs, |kCharacteristicDescriptionUuid|, and
  // |kCharacteristicPresentationFormat| are supported for locally hosted
  // descriptors. This method will return NULL if |uuid| is any one of the
  // unsupported predefined descriptor UUIDs.
  static BluetoothGattDescriptor* Create(const bluetooth_utils::UUID& uuid,
                                         const std::vector<uint8>& value);

  // The Bluetooth-specific UUID of the characteristic descriptor.
  virtual const bluetooth_utils::UUID& GetUuid() const = 0;

  // Returns true, if this characteristic descriptor is hosted locally. If
  // false, then this instance represents a remote descriptor.
  virtual bool IsLocal() const = 0;

  // Returns a pointer to the GATT characteristic that this characteristic
  // descriptor belongs to.
  virtual const BluetoothGattCharacteristic* GetCharacteristic() const = 0;

  // Sends a read request to a remote characteristic descriptor to read its
  // value. |callback| is called to return the read value on success and
  // |error_callback| is called for failures.
  virtual void ReadRemoteDescriptor(const ValueCallback& callback,
                                    const ErrorCallback& error_callback) = 0;

  // Sends a write request to a remote characteristic descriptor, to modify the
  // value of the descriptor starting at offset |offset| with the new value
  // |new_value|. |callback| is called to signal success and |error_callback|
  // for failures. This method only applies to remote descriptors and will fail
  // for those that are locally hosted.
  virtual void WriteRemoteDescriptor(
      int offset,
      const std::vector<uint8>& new_value,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

 protected:
  BluetoothGattDescriptor();
  virtual ~BluetoothGattDescriptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothGattDescriptor);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_GATT_DESCRIPTOR_H_
