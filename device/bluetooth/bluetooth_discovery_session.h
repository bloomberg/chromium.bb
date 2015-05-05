// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {

// used to keep discovery filter that might be Used to limit reported devices.
class DEVICE_BLUETOOTH_EXPORT BluetoothDiscoveryFilter {
 public:
  // Possible transports to use for scan filter.
  enum Transport {
    TRANSPORT_CLASSIC = 0x01,
    TRANSPORT_LE = 0x02,
    TRANSPORT_DUAL = (TRANSPORT_CLASSIC | TRANSPORT_LE)
  };
  using TransportMask = uint8_t;

  BluetoothDiscoveryFilter(TransportMask transport);
  ~BluetoothDiscoveryFilter();

  // These getters return true when given field is set in filter, and copy this
  // value to |out_*| parameter. If value is not set, returns false.
  // Thes setters assign given value to proper filter field.
  bool GetRSSI(int16_t* out_rssi) const;
  void SetRSSI(int16_t rssi);
  bool GetPathloss(uint16_t* out_pathloss) const;
  void SetPathloss(uint16_t pathloss);

  // Return and set transport field of this filter.
  TransportMask GetTransport() const;
  void SetTransport(TransportMask transport);

  // Make |out_uuids| represent all uuids assigned to this filter.
  void GetUUIDs(std::set<device::BluetoothUUID>& out_uuids) const;

  // Add UUID to internal UUIDs filter. If UUIDs filter doesn't exist, it will
  // be created.
  void AddUUID(const device::BluetoothUUID& uuid);

  // Copy content of |filter| and assigns it to this filter.
  void CopyFrom(const BluetoothDiscoveryFilter& filter);

  // Check if two filters are equal.
  bool Equals(const BluetoothDiscoveryFilter& filter) const;

  // Returns true if all fields in filter are empty
  bool IsDefault() const;

  // Returns result of merging two filters together. If at least one of the
  // filters is NULL this will return an empty filter
  static scoped_ptr<device::BluetoothDiscoveryFilter> Merge(
      const device::BluetoothDiscoveryFilter* filter_a,
      const device::BluetoothDiscoveryFilter* filter_b);

 private:
  scoped_ptr<int16_t> rssi_;
  scoped_ptr<uint16_t> pathloss_;
  TransportMask transport_;
  ScopedVector<device::BluetoothUUID> uuids_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDiscoveryFilter);
};

// BluetoothDiscoverySession represents a current active or inactive device
// discovery session. Instances of this class are obtained by calling
// BluetoothAdapter::StartDiscoverySession. The Bluetooth adapter will be
// constantly searching for nearby devices, as long as at least one instance
// of an active BluetoothDiscoverySession exists. A BluetoothDiscoverySession is
// considered active, as long as the adapter is discovering AND the owner of the
// instance has not called BluetoothDiscoverySession::Stop. A
// BluetoothDiscoverySession might unexpectedly become inactive, if the adapter
// unexpectedly stops discovery. Users can implement the
// AdapterDiscoveringChanged method of the BluetoothAdapter::Observer interface
// to be notified of such a change and promptly request a new
// BluetoothDiscoverySession if their existing sessions have become inactive.
class DEVICE_BLUETOOTH_EXPORT BluetoothDiscoverySession {
 public:
  // The ErrorCallback is used by methods to asynchronously report errors.
  typedef base::Closure ErrorCallback;

  // Destructor automatically terminates the discovery session. If this
  // results in a call to the underlying system to stop device discovery
  // (i.e. this instance represents the last active discovery session),
  // the call may not always succeed. To be notified of such failures,
  // users are highly encouraged to call BluetoothDiscoverySession::Stop,
  // instead of relying on the destructor.
  virtual ~BluetoothDiscoverySession();

  // Returns true if the session is active, false otherwise. If false, the
  // adapter might still be discovering as there might still be other active
  // sessions; this just means that this instance no longer has a say in
  // whether or not discovery should continue. In this case, the application
  // should request a new BluetoothDiscoverySession to make sure that device
  // discovery continues.
  virtual bool IsActive() const;

  // Requests this discovery session instance to stop. If this instance is
  // active, the session will stop. On success, |callback| is called and
  // on error |error_callback| is called. After a successful invocation, the
  // adapter may or may not stop device discovery, depending on whether or not
  // other active discovery sessions are present. Users are highly encouraged
  // to call this method to end a discovery session, instead of relying on the
  // destructor, so that they can be notified of the result via the callback
  // arguments.
  virtual void Stop(const base::Closure& callback,
                    const ErrorCallback& error_callback);

  virtual void SetDiscoveryFilter(
      scoped_ptr<BluetoothDiscoveryFilter> discovery_filter,
      const base::Closure& callback,
      const ErrorCallback& error_callback);

  virtual const BluetoothDiscoveryFilter* GetDiscoveryFilter() const;

 protected:
  explicit BluetoothDiscoverySession(
      scoped_refptr<BluetoothAdapter> adapter,
      scoped_ptr<BluetoothDiscoveryFilter> discovery_filter);

 private:
  friend class BluetoothAdapter;

  // Internal callback invoked when a call to
  // BluetoothAdapter::RemoveDiscoverySession has succeeded. Invokes
  // |deactivate_discovery_session| if the session object still
  // exists when this callback executes. Always invokes |success_callback|.
  static void OnDiscoverySessionRemoved(
      const base::Closure& deactivate_discovery_session,
      const base::Closure& success_callback);

  // Deactivate discovery session object after
  // BluetoothAdapter::RemoveDiscoverySession completes.
  void DeactivateDiscoverySession();

  // Marks this instance as inactive. Called by BluetoothAdapter to mark a
  // session as inactive in the case of an unexpected change to the adapter
  // discovery state.
  void MarkAsInactive();

  // Whether or not this instance represents an active discovery session.
  bool active_;

  // The adapter that created this instance.
  scoped_refptr<BluetoothAdapter> adapter_;

  // Filter assigned to this session, if any
  scoped_ptr<BluetoothDiscoveryFilter> discovery_filter_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDiscoverySession> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDiscoverySession);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DISCOVERY_SESSION_H_
