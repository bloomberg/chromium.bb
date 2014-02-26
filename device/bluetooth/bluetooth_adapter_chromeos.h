// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_

#include <queue>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothAdapterFactory;

}  // namespace device

namespace chromeos {

class BluetoothChromeOSTest;
class BluetoothDeviceChromeOS;

// The BluetoothAdapterChromeOS class implements BluetoothAdapter for the
// Chrome OS platform.
class BluetoothAdapterChromeOS
    : public device::BluetoothAdapter,
      private chromeos::BluetoothAdapterClient::Observer,
      private chromeos::BluetoothDeviceClient::Observer,
      private chromeos::BluetoothInputClient::Observer,
      private chromeos::BluetoothAgentServiceProvider::Delegate {
 public:
  // BluetoothAdapter override
  virtual void AddObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      device::BluetoothAdapter::Observer* observer) OVERRIDE;
  virtual std::string GetAddress() const OVERRIDE;
  virtual std::string GetName() const OVERRIDE;
  virtual void SetName(const std::string& name,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsInitialized() const OVERRIDE;
  virtual bool IsPresent() const OVERRIDE;
  virtual bool IsPowered() const OVERRIDE;
  virtual void SetPowered(
      bool powered,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscoverable() const OVERRIDE;
  virtual void SetDiscoverable(
      bool discoverable,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool IsDiscovering() const OVERRIDE;
  virtual void ReadLocalOutOfBandPairingData(
      const device::BluetoothAdapter::BluetoothOutOfBandPairingDataCallback&
          callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class device::BluetoothAdapterFactory;
  friend class BluetoothChromeOSTest;
  friend class BluetoothDeviceChromeOS;
  friend class BluetoothProfileChromeOS;
  friend class BluetoothProfileChromeOSTest;

  // typedef for callback parameters that are passed to AddDiscoverySession
  // and RemoveDiscoverySession. This is used to queue incoming requests while
  // a call to BlueZ is pending.
  typedef std::pair<base::Closure, ErrorCallback> DiscoveryCallbackPair;
  typedef std::queue<DiscoveryCallbackPair> DiscoveryCallbackQueue;

  BluetoothAdapterChromeOS();
  virtual ~BluetoothAdapterChromeOS();

  // BluetoothAdapterClient::Observer override.
  virtual void AdapterAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void AdapterPropertyChanged(
      const dbus::ObjectPath& object_path,
      const std::string& property_name) OVERRIDE;

  // BluetoothDeviceClient::Observer override.
  virtual void DeviceAdded(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DeviceRemoved(const dbus::ObjectPath& object_path) OVERRIDE;
  virtual void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                                     const std::string& property_name) OVERRIDE;

  // BluetoothInputClient::Observer override.
  virtual void InputPropertyChanged(const dbus::ObjectPath& object_path,
                                    const std::string& property_name) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
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

  // PairingContext is an API between BluetoothAdapterChromeOS and
  // BluetoothDeviceChromeOS for a single pairing attempt, wrapping the
  // callbacks of the underlying BluetoothAgentServiceProvider object.
  class PairingContext {
   public:
    ~PairingContext();

    // Indicates whether the device is currently pairing and expecting a
    // PIN Code to be returned.
    bool ExpectingPinCode() const;

    // Indicates whether the device is currently pairing and expecting a
    // Passkey to be returned.
    bool ExpectingPasskey() const;

    // Indicates whether the device is currently pairing and expecting
    // confirmation of a displayed passkey.
    bool ExpectingConfirmation() const;

    // Sends the PIN code |pincode| to the remote device during pairing.
    //
    // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
    // for which there is no automatic pairing or special handling.
    void SetPinCode(const std::string& pincode);

    // Sends the Passkey |passkey| to the remote device during pairing.
    //
    // Passkeys are generally required for Bluetooth 2.1 and later devices
    // which cannot provide input or display on their own, and don't accept
    // passkey-less pairing, and are a numeric in the range 0-999999.
    void SetPasskey(uint32 passkey);

    // Confirms to the remote device during pairing that a passkey provided by
    // the ConfirmPasskey() delegate call is displayed on both devices.
    void ConfirmPairing();

    // Rejects a pairing or connection request from a remote device, returns
    // false if there was no way to reject the pairing.
    bool RejectPairing();

    // Cancels a pairing or connection attempt to a remote device, returns
    // false if there was no way to cancel the pairing.
    bool CancelPairing();

   private:
    friend class BluetoothAdapterChromeOS;
    friend class BluetoothDeviceChromeOS;

    explicit PairingContext(
        device::BluetoothDevice::PairingDelegate* pairing_delegate_);

    // Internal method to response to the relevant callback for a RejectPairing
    // or CancelPairing call.
    bool RunPairingCallbacks(
        BluetoothAgentServiceProvider::Delegate::Status status);

    // UI Pairing Delegate to make method calls on, this must live as long as
    // the object capturing the PairingContext.
    device::BluetoothDevice::PairingDelegate* pairing_delegate_;

    // Flag to indicate whether any pairing delegate method has been called
    // during pairing. Used to determine whether we need to log the
    // "no pairing interaction" metric.
    bool pairing_delegate_used_;

    // During pairing these callbacks are set to those provided by method calls
    // made on the BluetoothAdapterChromeOS instance by its respective
    // BluetoothAgentServiceProvider instance, and are called by our own
    // method calls such as SetPinCode() and SetPasskey().
    PinCodeCallback pincode_callback_;
    PasskeyCallback passkey_callback_;
    ConfirmationCallback confirmation_callback_;
  };

  // Called by dbus:: on completion of the D-Bus method call to register the
  // pairing agent.
  void OnRegisterAgent();
  void OnRegisterAgentError(const std::string& error_name,
                            const std::string& error_message);

  // Internal method used to locate the device object by object path
  // (the devices map and BluetoothDevice methods are by address)
  BluetoothDeviceChromeOS* GetDeviceWithPath(
      const dbus::ObjectPath& object_path);

  // Internal method to obtain the ChromeOS BluetoothDevice object, returned in
  // |device_chromeos| and associated PairingContext, returned in
  // |pairing_context| for the device with path |object_path|.
  // Returns true on success, false if device doesn't exist or there is no
  // pairing context for it.
  bool GetDeviceAndPairingContext(const dbus::ObjectPath& object_path,
                                  BluetoothDeviceChromeOS** device_chromeos,
                                  PairingContext** pairing_context);

  // Set the tracked adapter to the one in |object_path|, this object will
  // subsequently operate on that adapter until it is removed.
  void SetAdapter(const dbus::ObjectPath& object_path);

  // Set the adapter name to one chosen from the system information.
  void SetDefaultAdapterName();

  // Remove the currently tracked adapter. IsPresent() will return false after
  // this is called.
  void RemoveAdapter();

  // Announce to observers a change in the adapter state.
  void PoweredChanged(bool powered);
  void DiscoverableChanged(bool discoverable);
  void DiscoveringChanged(bool discovering);
  void PresentChanged(bool present);

  // Announce to observers a change in device state that is not reflected by
  // its D-Bus properties.
  void NotifyDeviceChanged(BluetoothDeviceChromeOS* device);

  // Called by dbus:: on completion of the discoverable property change.
  void OnSetDiscoverable(const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         bool success);

  // Called by dbus:: on completion of an adapter property change.
  void OnPropertyChangeCompleted(const base::Closure& callback,
                                 const ErrorCallback& error_callback,
                                 bool success);

  // BluetoothAdapter override.
  virtual void AddDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void RemoveDiscoverySession(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

  // Called by dbus:: on completion of the D-Bus method call to start discovery.
  void OnStartDiscovery(const base::Closure& callback);
  void OnStartDiscoveryError(const ErrorCallback& error_callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop discovery.
  void OnStopDiscovery(const base::Closure& callback);
  void OnStopDiscoveryError(const ErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Processes the queued discovery requests. For each DiscoveryCallbackPair in
  // the queue, this method will try to add a new discovery session. This method
  // is called whenever a pending D-Bus call to start or stop discovery has
  // ended (with either success or failure).
  void ProcessQueuedDiscoveryRequests();

  // Number of discovery sessions that have been added.
  int num_discovery_sessions_;

  // True, if there is a pending request to start or stop discovery.
  bool discovery_request_pending_;

  // List of queued requests to add new discovery sessions. While there is a
  // pending request to BlueZ to start or stop discovery, many requests from
  // within Chrome to start or stop discovery sessions may occur. We only
  // queue requests to add new sessions to be processed later. All requests to
  // remove a session while a call is pending immediately return failure. Note
  // that since BlueZ keeps its own reference count of applications that have
  // requested discovery, dropping our count to 0 won't necessarily result in
  // the controller actually stopping discovery if, for example, an application
  // other than Chrome, such as bt_console, was also used to start discovery.
  //
  // TODO(armansito): With the new API, it will not be possible to have requests
  // to remove a discovery session while a call is pending. If the pending
  // request is to start discovery, |num_discovery_sessions_| is 0. Since no
  // active instance of DiscoverySession exists, clients can only make calls to
  // request new sessions. Likewise, if the pending request is to stop
  // discovery, |num_discovery_sessions_| is 1 and we're currently processing
  // the request to stop the last active DiscoverySession. We should make sure
  // that this invariant holds via asserts once we implement DiscoverySession
  // and have fully removed the deprecated methods. For now, just return an
  // error in the removal case to support the deprecated methods. (See
  // crbug.com/3445008).
  DiscoveryCallbackQueue discovery_request_queue_;

  // Object path of the adapter we track.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from us.
  ObserverList<device::BluetoothAdapter::Observer> observers_;

  // Instance of the D-Bus agent object used for pairing, initialized with
  // our own class as its delegate.
  scoped_ptr<BluetoothAgentServiceProvider> agent_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
