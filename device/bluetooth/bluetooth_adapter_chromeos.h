// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_

#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/dbus/bluetooth_adapter_client.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_input_client.h"
#include "chromeos/dbus/bluetooth_profile_manager_client.h"
#include "chromeos/dbus/bluetooth_profile_service_provider.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_audio_sink.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_export.h"

namespace device {
class BluetoothSocketThread;
}  // namespace device

namespace chromeos {

class BluetoothChromeOSTest;
class BluetoothAdapterProfileChromeOS;
class BluetoothDeviceChromeOS;
class BluetoothPairingChromeOS;
class BluetoothRemoteGattCharacteristicChromeOS;
class BluetoothRemoteGattDescriptorChromeOS;
class BluetoothRemoteGattServiceChromeOS;

// The BluetoothAdapterChromeOS class implements BluetoothAdapter for the
// Chrome OS platform.
//
// Methods tolerate a shutdown scenario where BluetoothAdapterChromeOS::Shutdown
// causes IsPresent to return false just before the dbus system is shutdown but
// while references to the BluetoothAdapterChromeOS object still exists.
//
// When adding methods to this class verify shutdown behavior in
// BluetoothChromeOSTest, Shutdown.
class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterChromeOS
    : public device::BluetoothAdapter,
      public chromeos::BluetoothAdapterClient::Observer,
      public chromeos::BluetoothDeviceClient::Observer,
      public chromeos::BluetoothInputClient::Observer,
      public chromeos::BluetoothAgentServiceProvider::Delegate {
 public:
  typedef base::Callback<void(const std::string& error_message)>
      ErrorCompletionCallback;
  typedef base::Callback<void(BluetoothAdapterProfileChromeOS* profile)>
      ProfileRegisteredCallback;

  static base::WeakPtr<BluetoothAdapter> CreateAdapter();

  // BluetoothAdapter:
  void Shutdown() override;
  void DeleteOnCorrectThread() const override;
  void AddObserver(device::BluetoothAdapter::Observer* observer) override;
  void RemoveObserver(device::BluetoothAdapter::Observer* observer) override;
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool powered,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       const base::Closure& callback,
                       const ErrorCallback& error_callback) override;
  bool IsDiscovering() const override;
  void CreateRfcommService(
      const device::BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void CreateL2capService(
      const device::BluetoothUUID& uuid,
      const ServiceOptions& options,
      const CreateServiceCallback& callback,
      const CreateServiceErrorCallback& error_callback) override;
  void RegisterAudioSink(
      const device::BluetoothAudioSink::Options& options,
      const device::BluetoothAdapter::AcquiredCallback& callback,
      const device::BluetoothAudioSink::ErrorCallback& error_callback) override;

  // Locates the device object by object path (the devices map and
  // BluetoothDevice methods are by address).
  BluetoothDeviceChromeOS* GetDeviceWithPath(
      const dbus::ObjectPath& object_path);

  // Announce to observers a change in device state that is not reflected by
  // its D-Bus properties.
  void NotifyDeviceChanged(BluetoothDeviceChromeOS* device);

  // The following methods are used to send various GATT observer events to
  // observers.
  void NotifyGattServiceAdded(BluetoothRemoteGattServiceChromeOS* service);
  void NotifyGattServiceRemoved(BluetoothRemoteGattServiceChromeOS* service);
  void NotifyGattServiceChanged(BluetoothRemoteGattServiceChromeOS* service);
  void NotifyGattDiscoveryComplete(BluetoothRemoteGattServiceChromeOS* service);
  void NotifyGattCharacteristicAdded(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic);
  void NotifyGattCharacteristicRemoved(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic);
  void NotifyGattDescriptorAdded(
      BluetoothRemoteGattDescriptorChromeOS* descriptor);
  void NotifyGattDescriptorRemoved(
      BluetoothRemoteGattDescriptorChromeOS* descriptor);
  void NotifyGattCharacteristicValueChanged(
      BluetoothRemoteGattCharacteristicChromeOS* characteristic,
      const std::vector<uint8>& value);
  void NotifyGattDescriptorValueChanged(
      BluetoothRemoteGattDescriptorChromeOS* descriptor,
      const std::vector<uint8>& value);

  // Returns the object path of the adapter.
  const dbus::ObjectPath& object_path() const { return object_path_; }

  // Request a profile on the adapter for a custom service with a
  // specific UUID for the device at |device_path| to be sent to |delegate|.
  // If |device_path| is the empty string, incoming connections will be
  // assigned to |delegate|.  When the profile is
  // successfully registered, |success_callback| will be called with a pointer
  // to the profile which is managed by BluetoothAdapterChromeOS.  On failure,
  // |error_callback| will be called.
  void UseProfile(const device::BluetoothUUID& uuid,
                  const dbus::ObjectPath& device_path,
                  const BluetoothProfileManagerClient::Options& options,
                  BluetoothProfileServiceProvider::Delegate* delegate,
                  const ProfileRegisteredCallback& success_callback,
                  const ErrorCompletionCallback& error_callback);

  // Release use of a profile by a device.
  void ReleaseProfile(const dbus::ObjectPath& device_path,
                      BluetoothAdapterProfileChromeOS* profile);

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class base::DeleteHelper<BluetoothAdapterChromeOS>;
  friend class BluetoothChromeOSTest;
  friend class BluetoothChromeOSTest_Shutdown_Test;
  friend class BluetoothChromeOSTest_Shutdown_OnStartDiscovery_Test;
  friend class BluetoothChromeOSTest_Shutdown_OnStartDiscoveryError_Test;
  friend class BluetoothChromeOSTest_Shutdown_OnStopDiscovery_Test;
  friend class BluetoothChromeOSTest_Shutdown_OnStopDiscoveryError_Test;

  // typedef for callback parameters that are passed to AddDiscoverySession
  // and RemoveDiscoverySession. This is used to queue incoming requests while
  // a call to BlueZ is pending.
  typedef std::pair<base::Closure, ErrorCallback> DiscoveryCallbackPair;
  typedef std::queue<DiscoveryCallbackPair> DiscoveryCallbackQueue;

  BluetoothAdapterChromeOS();
  ~BluetoothAdapterChromeOS() override;

  // BluetoothAdapterClient::Observer override.
  void AdapterAdded(const dbus::ObjectPath& object_path) override;
  void AdapterRemoved(const dbus::ObjectPath& object_path) override;
  void AdapterPropertyChanged(const dbus::ObjectPath& object_path,
                              const std::string& property_name) override;

  // BluetoothDeviceClient::Observer override.
  void DeviceAdded(const dbus::ObjectPath& object_path) override;
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // BluetoothInputClient::Observer override.
  void InputPropertyChanged(const dbus::ObjectPath& object_path,
                            const std::string& property_name) override;

  // BluetoothAgentServiceProvider::Delegate override.
  void Released() override;
  void RequestPinCode(const dbus::ObjectPath& device_path,
                      const PinCodeCallback& callback) override;
  void DisplayPinCode(const dbus::ObjectPath& device_path,
                      const std::string& pincode) override;
  void RequestPasskey(const dbus::ObjectPath& device_path,
                      const PasskeyCallback& callback) override;
  void DisplayPasskey(const dbus::ObjectPath& device_path,
                      uint32 passkey,
                      uint16 entered) override;
  void RequestConfirmation(const dbus::ObjectPath& device_path,
                           uint32 passkey,
                           const ConfirmationCallback& callback) override;
  void RequestAuthorization(const dbus::ObjectPath& device_path,
                            const ConfirmationCallback& callback) override;
  void AuthorizeService(const dbus::ObjectPath& device_path,
                        const std::string& uuid,
                        const ConfirmationCallback& callback) override;
  void Cancel() override;

  // Called by dbus:: on completion of the D-Bus method call to register the
  // pairing agent.
  void OnRegisterAgent();
  void OnRegisterAgentError(const std::string& error_name,
                            const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to request that
  // the pairing agent be made the default.
  void OnRequestDefaultAgent();
  void OnRequestDefaultAgentError(const std::string& error_name,
                                  const std::string& error_message);

  // Called by BluetoothAudioSinkChromeOS on completion of registering an audio
  // sink.
  void OnRegisterAudioSink(
      const device::BluetoothAdapter::AcquiredCallback& callback,
      const device::BluetoothAudioSink::ErrorCallback& error_callback,
      scoped_refptr<device::BluetoothAudioSink> audio_sink);

  // Internal method to obtain a BluetoothPairingChromeOS object for the device
  // with path |object_path|. Returns the existing pairing object if the device
  // already has one (usually an outgoing connection in progress) or a new
  // pairing object with the default pairing delegate if not. If no default
  // pairing object exists, NULL will be returned.
  BluetoothPairingChromeOS* GetPairing(const dbus::ObjectPath& object_path);

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

  // Called by dbus:: on completion of the discoverable property change.
  void OnSetDiscoverable(const base::Closure& callback,
                         const ErrorCallback& error_callback,
                         bool success);

  // Called by dbus:: on completion of an adapter property change.
  void OnPropertyChangeCompleted(const base::Closure& callback,
                                 const ErrorCallback& error_callback,
                                 bool success);

  // BluetoothAdapter:
  void AddDiscoverySession(const base::Closure& callback,
                           const ErrorCallback& error_callback) override;
  void RemoveDiscoverySession(const base::Closure& callback,
                              const ErrorCallback& error_callback) override;

  // Called by dbus:: on completion of the D-Bus method call to start discovery.
  void OnStartDiscovery(const base::Closure& callback,
                        const ErrorCallback& error_callback);
  void OnStartDiscoveryError(const base::Closure& callback,
                             const ErrorCallback& error_callback,
                             const std::string& error_name,
                             const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method call to stop discovery.
  void OnStopDiscovery(const base::Closure& callback);
  void OnStopDiscoveryError(const ErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Called by dbus:: on completion of the D-Bus method to register a profile.
  void OnRegisterProfile(const device::BluetoothUUID& uuid,
                         BluetoothAdapterProfileChromeOS* profile);

  void SetProfileDelegate(const device::BluetoothUUID& uuid,
                          const dbus::ObjectPath& device_path,
                          BluetoothProfileServiceProvider::Delegate* delegate,
                          const ProfileRegisteredCallback& success_callback,
                          const ErrorCompletionCallback& error_callback);
  void OnRegisterProfileError(const device::BluetoothUUID& uuid,
                              const std::string& error_name,
                              const std::string& error_message);

  // Called by BluetoothAdapterProfileChromeOS when no users of a profile
  // remain.
  void RemoveProfile(const device::BluetoothUUID& uuid);

  // Processes the queued discovery requests. For each DiscoveryCallbackPair in
  // the queue, this method will try to add a new discovery session. This method
  // is called whenever a pending D-Bus call to start or stop discovery has
  // ended (with either success or failure).
  void ProcessQueuedDiscoveryRequests();

  // Set in |Shutdown()|, makes IsPresent()| return false.
  bool dbus_is_shutdown_;

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
  DiscoveryCallbackQueue discovery_request_queue_;

  // Object path of the adapter we track.
  dbus::ObjectPath object_path_;

  // List of observers interested in event notifications from us.
  ObserverList<device::BluetoothAdapter::Observer> observers_;

  // Instance of the D-Bus agent object used for pairing, initialized with
  // our own class as its delegate.
  scoped_ptr<BluetoothAgentServiceProvider> agent_;

  // UI thread task runner and socket thread object used to create sockets.
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<device::BluetoothSocketThread> socket_thread_;

  // The profiles we have registered with the bluetooth daemon.
  std::map<device::BluetoothUUID, BluetoothAdapterProfileChromeOS*> profiles_;

  // Callback pair for the profile registration queue.
  typedef std::pair<base::Closure, ErrorCompletionCallback>
      RegisterProfileCompletionPair;

  // Queue of delegates waiting for a profile to register.
  std::map<device::BluetoothUUID, std::vector<RegisterProfileCompletionPair>*>
      profile_queues_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothAdapterChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_CHROMEOS_H_
