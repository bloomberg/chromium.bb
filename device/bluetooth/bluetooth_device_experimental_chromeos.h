// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_EXPERIMENTAL_CHROMEOS_H
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_EXPERIMENTAL_CHROMEOS_H

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/experimental_bluetooth_agent_service_provider.h"
#include "chromeos/dbus/experimental_bluetooth_device_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"

namespace chromeos {

class BluetoothAdapterExperimentalChromeOS;

// The BluetoothDeviceExperimentalChromeOS class is an alternate implementation
// of BluetoothDevice for the Chrome OS platform using the Bluetooth Smart
// capable backend. It will become the sole implementation for Chrome OS, and
// be renamed to BluetoothDeviceChromeOS, once the backend is switched.
class BluetoothDeviceExperimentalChromeOS
    : public device::BluetoothDevice,
      private chromeos::ExperimentalBluetoothAgentServiceProvider::Delegate {
 public:
  // BluetoothDevice override
  virtual uint32 GetBluetoothClass() const OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual uint16 GetVendorID() const OVERRIDE;
  virtual uint16 GetProductID() const OVERRIDE;
  virtual uint16 GetDeviceID() const OVERRIDE;
  virtual bool IsPaired() const OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectable() const OVERRIDE;
  virtual bool IsConnecting() const OVERRIDE;
  virtual ServiceList GetServices() const OVERRIDE;
  virtual void GetServiceRecords(
      const ServiceRecordsCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ProvidesServiceWithName(
      const std::string& name,
      const ProvidesServiceCallback& callback) OVERRIDE;
  virtual bool ExpectingPinCode() const OVERRIDE;
  virtual bool ExpectingPasskey() const OVERRIDE;
  virtual bool ExpectingConfirmation() const OVERRIDE;
  virtual void Connect(
      device::BluetoothDevice::PairingDelegate* pairing_delegate,
      const base::Closure& callback,
      const ConnectErrorCallback& error_callback) OVERRIDE;
  virtual void SetPinCode(const std::string& pincode) OVERRIDE;
  virtual void SetPasskey(uint32 passkey) OVERRIDE;
  virtual void ConfirmPairing() OVERRIDE;
  virtual void RejectPairing() OVERRIDE;
  virtual void CancelPairing() OVERRIDE;
  virtual void Disconnect(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void Forget(const ErrorCallback& error_callback) OVERRIDE;
  virtual void ConnectToService(
      const std::string& service_uuid,
      const SocketCallback& callback) OVERRIDE;
  virtual void ConnectToProfile(
      device::BluetoothProfile* profile,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void SetOutOfBandPairingData(
      const device::BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

 protected:
   // BluetoothDevice override
  virtual std::string GetDeviceName() const OVERRIDE;

 private:
  friend class BluetoothAdapterExperimentalChromeOS;

  BluetoothDeviceExperimentalChromeOS(
      BluetoothAdapterExperimentalChromeOS* adapter,
      const dbus::ObjectPath& object_path);
  virtual ~BluetoothDeviceExperimentalChromeOS();

  // ExperimentalBluetoothAgentServiceProvider::Delegate override.
  virtual void Release() OVERRIDE;
  virtual void RequestPinCode(const dbus::ObjectPath& device_path,
                              const PinCodeCallback& callback) OVERRIDE;
  virtual void DisplayPinCode(const dbus::ObjectPath& device_path,
                              const std::string& pincode) OVERRIDE;
  virtual void RequestPasskey(const dbus::ObjectPath& device_path,
                              const PasskeyCallback& callback) OVERRIDE;
  virtual void DisplayPasskey(const dbus::ObjectPath& device_path,
                              uint32 passkey, uint16 entered) OVERRIDE;
  virtual void RequestConfirmation(const dbus::ObjectPath& device_path,
                                   uint32 passkey,
                                   const ConfirmationCallback& callback)
      OVERRIDE;
  virtual void RequestAuthorization(const dbus::ObjectPath& device_path,
                                    const ConfirmationCallback& callback)
      OVERRIDE;
  virtual void AuthorizeService(const dbus::ObjectPath& device_path,
                                const std::string& uuid,
                                const ConfirmationCallback& callback) OVERRIDE;
  virtual void Cancel() OVERRIDE;

  // Internal method to initiate a connection to this device, and methods called
  // by dbus:: on completion of the D-Bus method call.
  void ConnectInternal(bool after_pairing,
                       const base::Closure& callback,
                       const ConnectErrorCallback& error_callback);
  void OnConnect(bool after_pairing,
                 const base::Closure& callback);
  void OnConnectError(bool after_pairing,
                      const ConnectErrorCallback& error_callback,
                      const std::string& error_name,
                      const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to register the
  // pairing agent.
  void OnRegisterAgent(const base::Closure& callback,
                       const ConnectErrorCallback& error_callback);
  void OnRegisterAgentError(const ConnectErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to pair the device.
  void OnPair(const base::Closure& callback,
              const ConnectErrorCallback& error_callback);
  void OnPairError(const ConnectErrorCallback& error_callback,
                   const std::string& error_name,
                   const std::string& error_message);

  // Called by dbus:: on failure of the D-Bus method call to cancel pairing,
  // there is no matching completion call since we don't do anything special
  // in that case.
  void OnCancelPairingError(const std::string& error_name,
                            const std::string& error_message);

  // Internal method to set the device as trusted. Trusted devices can connect
  // to us automatically, and we can connect to them after rebooting; it also
  // causes the device to be remembered by the stack even if not paired.
  // |success| to the callback indicates whether or not the request succeeded.
  void SetTrusted();
  void OnSetTrusted(bool success);

  // Internal method to unregister the pairing agent and method called by dbus::
  // on failure of the D-Bus method call. No completion call as success is
  // ignored.
  void UnregisterAgent();
  void OnUnregisterAgentError(const std::string& error_name,
                              const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to disconnect the
  // device.
  void OnDisconnect(const base::Closure& callback);
  void OnDisconnectError(const ErrorCallback& error_callback,
                         const std::string& error_name,
                         const std::string& error_message);

  // Called by dbus:: on failure of the D-Bus method call to unpair the device;
  // there is no matching completion call since this object is deleted in the
  // process of unpairing.
  void OnForgetError(const ErrorCallback& error_callback,
                     const std::string& error_name,
                     const std::string& error_message);

  // Run any outstanding pairing callbacks passing |status| as the result of
  // pairing. Returns true if any callbacks were run, false if not.
  bool RunPairingCallbacks(Status status);

  // Return the object path of the device; used by
  // BluetoothAdapterExperimentalChromeOS
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // The adapter that owns this device instance.
  BluetoothAdapterExperimentalChromeOS* adapter_;

  // The dbus object path of the device object.
  dbus::ObjectPath object_path_;

  // Number of ongoing calls to Connect().
  int num_connecting_calls_;

  // During pairing this is set to an object that we don't own, but on which
  // we can make method calls to request, display or confirm PIN Codes and
  // Passkeys. Generally it is the object that owns this one.
  PairingDelegate* pairing_delegate_;

  // Flag to indicate whether a pairing delegate method has been called during
  // pairing.
  bool pairing_delegate_used_;

  // During pairing this is set to an instance of a D-Bus agent object
  // intialized with our own class as its delegate.
  scoped_ptr<ExperimentalBluetoothAgentServiceProvider> agent_;

  // During pairing these callbacks are set to those provided by method calls
  // made on us by |agent_| and are called by our own method calls such as
  // SetPinCode() and SetPasskey().
  PinCodeCallback pincode_callback_;
  PasskeyCallback passkey_callback_;
  ConfirmationCallback confirmation_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceExperimentalChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceExperimentalChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_EXPERIMENTAL_CHROMEOS_H
