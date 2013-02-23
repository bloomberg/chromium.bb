// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_CHROMEOS_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_CHROMEOS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_device.h"

namespace device {

class BluetoothServiceRecord;
class MockBluetoothDevice;
struct BluetoothOutOfBandPairingData;

}  // namespace device

namespace chromeos {

class BluetoothAdapterChromeOS;

// The BluetoothDeviceChromeOS class is an implementation of BluetoothDevice
// for Chrome OS platform.
class BluetoothDeviceChromeOS
    : public device::BluetoothDevice,
      public BluetoothDeviceClient::Observer,
      public BluetoothAgentServiceProvider::Delegate {
 public:
  virtual ~BluetoothDeviceChromeOS();

  // BluetoothDevice override
  virtual bool IsPaired() const OVERRIDE;
  virtual const ServiceList& GetServices() const OVERRIDE;
  virtual void GetServiceRecords(
      const ServiceRecordsCallback& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual bool ProvidesServiceWithUUID(const std::string& uuid) const OVERRIDE;
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
  virtual void SetOutOfBandPairingData(
      const device::BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback) OVERRIDE;

 private:
  friend class BluetoothAdapterChromeOS;
  friend class device::MockBluetoothDevice;

  explicit BluetoothDeviceChromeOS(BluetoothAdapterChromeOS* adapter);

  // Sets the dbus object path for the device to |object_path|, indicating
  // that the device has gone from being discovered to paired or bonded.
  void SetObjectPath(const dbus::ObjectPath& object_path);

  // Removes the dbus object path from the device, indicating that the
  // device is no longer paired or bonded, but perhaps still visible.
  void RemoveObjectPath();

  // Sets whether the device is visible to the owning adapter to |visible|.
  void SetVisible(bool visible) { visible_ = visible; }

  // Updates device information from the properties in |properties|, device
  // state properties such as |paired_| and |connected_| are ignored unless
  // |update_state| is true.
  void Update(const BluetoothDeviceClient::Properties* properties,
              bool update_state);

  // Called by BluetoothAdapterClient when a call to CreateDevice() or
  // CreatePairedDevice() succeeds, provides the new object path for the remote
  // device in |device_path|. |callback| and |error_callback| are the callbacks
  // provided to Connect().
  void OnCreateDevice(const base::Closure& callback,
                      const ConnectErrorCallback& error_callback,
                      const dbus::ObjectPath& device_path);

  // Called by BluetoothAdapterClient when a call to CreateDevice() or
  // CreatePairedDevice() fails with the error named |error_name| and
  // optional message |error_message|, |error_callback| is the callback
  // provided to Connect().
  void OnCreateDeviceError(const ConnectErrorCallback& error_callback,
                           const std::string& error_name,
                           const std::string& error_message);

  // Called by BluetoothAdapterClient when a call to GetServiceRecords()
  // completes.  |callback| and |error_callback| are the callbacks provided to
  // GetServiceRecords.
  void CollectServiceRecordsCallback(
      const ServiceRecordsCallback& callback,
      const ErrorCallback& error_callback,
      const dbus::ObjectPath& device_path,
      const BluetoothDeviceClient::ServiceMap& service_map,
      bool success);

  // Called by CollectServiceRecordsCallback() every time |service_records_| is
  // updated. |OnServiceRecordsChanged| updates the derived properties that
  // depend on |service_records_|.
  void OnServiceRecordsChanged(void);

  // Called by BluetoothProperty when the call to Set() for the Trusted
  // property completes. |success| indicates whether or not the request
  // succeeded.
  void OnSetTrusted(bool success);

  // Called by BluetoothAdapterClient when a call to GetServiceRecords()
  // fails.  |callback| and |error_callback| are the callbacks provided to
  // GetServiceRecords().
  void OnGetServiceRecordsError(const ServiceRecordsCallback& callback,
                                const ErrorCallback& error_callback);

  // Called by BluetoothAdapterClient when the initial call to
  // GetServiceRecords() after pairing completes. |callback| and
  // |error_callback| are the callbacks provided to Connect().
  void OnInitialGetServiceRecords(const base::Closure& callback,
                                  const ConnectErrorCallback& error_callback,
                                  const ServiceRecordList& list);

  // Called by BluetoothAdapterClient when the initial call to
  // GetServiceRecords() after pairing fails. |callback| and |error_callback|
  // are the callbacks provided to Connect().
  void OnInitialGetServiceRecordsError(
      const base::Closure& callback,
      const ConnectErrorCallback& error_callback);

  // Connect application-level protocols of the device to the system, called
  // on a successful connection or to reconnect to a device that is already
  // paired or previously connected. |error_callback| is called on failure.
  // Otherwise, |callback| is called when the request is complete.
  void ConnectApplications(const base::Closure& callback,
                           const ConnectErrorCallback& error_callback);

  // Called by IntrospectableClient when a call to Introspect() completes.
  // |success| indicates whether or not the request succeeded, |callback| and
  // |error_callback| are the callbacks provided to ConnectApplications(),
  // |service_name| and |device_path| specify the remote object being
  // introspected and |xml_data| contains the XML-formatted protocol data.
  void OnIntrospect(const base::Closure& callback,
                    const ConnectErrorCallback& error_callback,
                    const std::string& service_name,
                    const dbus::ObjectPath& device_path,
                    const std::string& xml_data, bool success);

  // Called by BluetoothInputClient when the call to Connect() succeeds.
  // |error_callback| is the callback provided to ConnectApplications(),
  // |interface_name| specifies the interface being connected and
  // |device_path| the remote object path.
  void OnConnect(const base::Closure& callback,
                 const std::string& interface_name,
                 const dbus::ObjectPath& device_path);

  // Called by BluetoothInputClient when the call to Connect() fails.
  // |error_callback| is the callback provided to ConnectApplications(),
  // |interface_name| specifies the interface being connected,
  // |device_path| the remote object path,
  // |error_name| the error name and |error_message| the optional message.
  void OnConnectError(const ConnectErrorCallback& error_callback,
                      const std::string& interface_name,
                      const dbus::ObjectPath& device_path,
                      const std::string& error_name,
                      const std::string& error_message);

  // Called by BluetoothDeviceClient when a call to Disconnect() completes,
  // |success| indicates whether or not the request succeeded, |callback| and
  // |error_callback| are the callbacks provided to Disconnect() and
  // |device_path| is the device disconnected.
  void DisconnectCallback(const base::Closure& callback,
                          const ErrorCallback& error_callback,
                          const dbus::ObjectPath& device_path, bool success);

  // Called by BluetoothAdapterClient when a call to RemoveDevice()
  // completes, |success| indicates whether or not the request succeeded,
  // |error_callback| is the callback provided to Forget() and |adapter_path| is
  // the d-bus object path of the adapter that performed the removal.
  void ForgetCallback(const ErrorCallback& error_callback,
                      const dbus::ObjectPath& adapter_path, bool success);

  // Called by BluetoothAdapterClient when a call to CancelDeviceCreation()
  // completes, |success| indicates whether or not the request succeeded.
  void OnCancelDeviceCreation(const dbus::ObjectPath& adapter_path,
                              bool success);

  // Called if the call to GetServiceRecords from ProvidesServiceWithName fails.
  void SearchServicesForNameErrorCallback(
      const ProvidesServiceCallback& callback);

  // Called by GetServiceRecords with the list of BluetoothServiceRecords to
  // search for |name|.  |callback| is the callback from
  // ProvidesServiceWithName.
  void SearchServicesForNameCallback(
      const std::string& name,
      const ProvidesServiceCallback& callback,
      const ServiceRecordList& list);

  // Called if the call to GetServiceRecords from Connect fails.
  void GetServiceRecordsForConnectErrorCallback(
      const SocketCallback& callback);

  // Called by GetServiceRecords with the list of BluetoothServiceRecords.
  // Connections are attempted to each service in the list matching
  // |service_uuid|, and the socket from the first successful connection is
  // passed to |callback|.
  void GetServiceRecordsForConnectCallback(
      const std::string& service_uuid,
      const SocketCallback& callback,
      const ServiceRecordList& list);

  // Called by BlueoothDeviceClient in response to the AddRemoteData and
  // RemoveRemoteData method calls.
  void OnRemoteDataCallback(const base::Closure& callback,
                            const ErrorCallback& error_callback,
                            bool success);

  // BluetoothDeviceClient::Observer override.
  //
  // Called when the device with object path |object_path| is about
  // to be disconnected, giving a chance for application layers to
  // shut down cleanly.
  virtual void DisconnectRequested(
      const dbus::ObjectPath& object_path) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the agent is unregistered from the
  // Bluetooth daemon, generally at the end of a pairing request. It may be
  // used to perform cleanup tasks.
  virtual void Release() OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // PIN Code for authentication of the device with object path |device_path|,
  // the agent should obtain the code from the user and call |callback|
  // to provide it, or indicate rejection or cancellation of the request.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void RequestPinCode(const dbus::ObjectPath& device_path,
                              const PinCodeCallback& callback) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires a
  // Passkey for authentication of the device with object path |device_path|,
  // the agent should obtain the passkey from the user (a numeric in the
  // range 0-999999) and call |callback| to provide it, or indicate
  // rejection or cancellation of the request.
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing.
  virtual void RequestPasskey(const dbus::ObjectPath& device_path,
                              const PasskeyCallback& callback) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user enter the PIN code |pincode| into the device with object path
  // |device_path| so that it may be authenticated. The Cancel() method
  // will be called to dismiss the display once pairing is complete or
  // cancelled.
  //
  // This is used for Bluetooth 2.0 and earlier keyboard devices, the
  // |pincode| will always be a six-digit numeric in the range 000000-999999
  // for compatibilty with later specifications.
  virtual void DisplayPinCode(const dbus::ObjectPath& device_path,
                              const std::string& pincode) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user enter the Passkey |passkey| into the device with object path
  // |device_path| so that it may be authenticated. The Cancel() method
  // will be called to dismiss the display once pairing is complete or
  // cancelled.
  //
  // This is used for Bluetooth 2.1 and later devices that support input
  // but not display, such as keyboards. The Passkey is a numeric in the
  // range 0-999999 and should be always presented zero-padded to six
  // digits.
  virtual void DisplayPasskey(const dbus::ObjectPath& device_path,
                              uint32 passkey) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user confirm that the Passkey |passkey| is displayed on the screen
  // of the device with object path |object_path| so that it may be
  // authentication. The agent should display to the user and ask for
  // confirmation, then call |callback| to provide their response (success,
  // rejected or cancelled).
  //
  // This is used for Bluetooth 2.1 and later devices that support display,
  // such as other computers or phones. The Passkey is a numeric in the
  // range 0-999999 and should be always present zero-padded to six
  // digits.
  virtual void RequestConfirmation(
      const dbus::ObjectPath& device_path,
      uint32 passkey,
      const ConfirmationCallback& callback) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user confirm that the device with object path |object_path| is
  // authorized to connect to the service with UUID |uuid|. The agent should
  // confirm with the user and call |callback| to provide their response
  // (success, rejected or cancelled).
  virtual void Authorize(const dbus::ObjectPath& device_path,
                         const std::string& uuid,
                         const ConfirmationCallback& callback) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called when the Bluetooth daemon requires that the
  // user confirm that the device adapter may switch to mode |mode|. The
  // agent should confirm with the user and call |callback| to provide
  // their response (success, rejected or cancelled).
  virtual void ConfirmModeChange(Mode mode,
                                 const ConfirmationCallback& callback) OVERRIDE;

  // BluetoothAgentServiceProvider::Delegate override.
  //
  // This method will be called by the Bluetooth daemon to indicate that
  // the request failed before a reply was returned from the device.
  virtual void Cancel() OVERRIDE;

  // Creates a new BluetoothDeviceChromeOS object bound to the adapter
  // |adapter|.
  static BluetoothDeviceChromeOS* Create(BluetoothAdapterChromeOS* adapter);

  // The adapter that owns this device instance.
  BluetoothAdapterChromeOS* adapter_;

  // The dbus object path of the device, will be empty if the device has only
  // been discovered and not yet paired with.
  dbus::ObjectPath object_path_;

  // The services (identified by UUIDs) that this device provides.
  std::vector<std::string> service_uuids_;

  // During pairing this is set to an object that we don't own, but on which
  // we can make method calls to request, display or confirm PIN Codes and
  // Passkeys. Generally it is the object that owns this one.
  device::BluetoothDevice::PairingDelegate* pairing_delegate_;

  // During pairing this is set to an instance of a D-Bus agent object
  // intialized with our own class as its delegate.
  scoped_ptr<BluetoothAgentServiceProvider> agent_;

  // During pairing these callbacks are set to those provided by method calls
  // made on us by |agent_| and are called by our own method calls such as
  // SetPinCode() and SetPasskey().
  PinCodeCallback pincode_callback_;
  PasskeyCallback passkey_callback_;
  ConfirmationCallback confirmation_callback_;

  // Used to keep track of pending application connection requests.
  int connecting_applications_counter_;

  // A service records cache.
  ServiceRecordList service_records_;

  // This says whether the |service_records_| cache is initialized. Note that an
  // empty |service_records_| list can be a valid list.
  bool service_records_loaded_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceChromeOS> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDeviceChromeOS);
};

}  // namespace chromeos

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_CHROMEOS_H_
