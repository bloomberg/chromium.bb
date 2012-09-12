// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "chromeos/dbus/bluetooth_agent_service_provider.h"
#include "chromeos/dbus/bluetooth_device_client.h"
#include "chromeos/dbus/bluetooth_out_of_band_client.h"
#include "dbus/object_path.h"

namespace chromeos {

class BluetoothAdapter;
class BluetoothServiceRecord;
class BluetoothSocket;

// The BluetoothDevice class represents a remote Bluetooth device, both
// its properties and capabilities as discovered by a local adapter and
// actions that may be performed on the remove device such as pairing,
// connection and disconnection.
//
// The class is instantiated and managed by the BluetoothAdapter class
// and pointers should only be obtained from that class and not cached,
// instead use the address() method as a unique key for a device.
//
// Since the lifecycle of BluetoothDevice instances is managed by
// BluetoothAdapter, that class rather than this provides observer methods
// for devices coming and going, as well as properties being updated.
class BluetoothDevice : public BluetoothDeviceClient::Observer,
                        public BluetoothAgentServiceProvider::Delegate {
 public:
  // Possible values that may be returned by GetDeviceType(), representing
  // different types of bluetooth device that we support or are aware of
  // decoded from the bluetooth class information.
  enum DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_COMPUTER,
    DEVICE_PHONE,
    DEVICE_MODEM,
    DEVICE_PERIPHERAL,
    DEVICE_JOYSTICK,
    DEVICE_GAMEPAD,
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    DEVICE_TABLET,
    DEVICE_KEYBOARD_MOUSE_COMBO
  };

  // Interface for observing changes from bluetooth devices.
  class Observer {
   public:
    virtual ~Observer() {}

    // TODO(keybuk): add observers for pairing and connection.
  };

  // Interface for negotiating pairing of bluetooth devices.
  class PairingDelegate {
   public:
    virtual ~PairingDelegate() {}

    // This method will be called when the Bluetooth daemon requires a
    // PIN Code for authentication of the device |device|, the delegate should
    // obtain the code from the user and call SetPinCode() on the device to
    // provide it, or RejectPairing() or CancelPairing() to reject or cancel
    // the request.
    //
    // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
    // for which there is no automatic pairing or special handling.
    virtual void RequestPinCode(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires a
    // Passkey for authentication of the device |device|, the delegate should
    // obtain the passkey from the user (a numeric in the range 0-999999) and
    // call SetPasskey() on the device to provide it, or RejectPairing() or
    // CancelPairing() to reject or cancel the request.
    //
    // Passkeys are generally required for Bluetooth 2.1 and later devices
    // which cannot provide input or display on their own, and don't accept
    // passkey-less pairing.
    virtual void RequestPasskey(BluetoothDevice* device) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the PIN code |pincode| into the device |device| so that it
    // may be authenticated. The DismissDisplayOrConfirm() method
    // will be called to dismiss the display once pairing is complete or
    // cancelled.
    //
    // This is used for Bluetooth 2.0 and earlier keyboard devices, the
    // |pincode| will always be a six-digit numeric in the range 000000-999999
    // for compatibilty with later specifications.
    virtual void DisplayPinCode(BluetoothDevice* device,
                                const std::string& pincode) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user enter the Passkey |passkey| into the device |device| so that it
    // may be authenticated. The DismissDisplayOrConfirm() method will be
    // called to dismiss the display once pairing is complete or cancelled.
    //
    // This is used for Bluetooth 2.1 and later devices that support input
    // but not display, such as keyboards. The Passkey is a numeric in the
    // range 0-999999 and should be always presented zero-padded to six
    // digits.
    virtual void DisplayPasskey(BluetoothDevice* device,
                                uint32 passkey) = 0;

    // This method will be called when the Bluetooth daemon requires that the
    // user confirm that the Passkey |passkey| is displayed on the screen
    // of the device |device| so that it may be authenticated. The delegate
    // should display to the user and ask for confirmation, then call
    // ConfirmPairing() on the device to confirm, RejectPairing() on the device
    // to reject or CancelPairing() on the device to cancel authentication
    // for any other reason.
    //
    // This is used for Bluetooth 2.1 and later devices that support display,
    // such as other computers or phones. The Passkey is a numeric in the
    // range 0-999999 and should be always present zero-padded to six
    // digits.
    virtual void ConfirmPasskey(BluetoothDevice* device,
                                uint32 passkey) = 0;

    // This method will be called when any previous DisplayPinCode(),
    // DisplayPasskey() or ConfirmPasskey() request should be concluded
    // and removed from the user.
    virtual void DismissDisplayOrConfirm() = 0;
  };

  virtual ~BluetoothDevice();

  // Returns the Bluetooth of address the device. This should be used as
  // a unique key to identify the device and copied where needed.
  virtual const std::string& address() const;

  // Returns the name of the device suitable for displaying, this may
  // be a synthesied string containing the address and localized type name
  // if the device has no obtained name.
  virtual string16 GetName() const;

  // Returns the type of the device, limited to those we support or are
  // aware of, by decoding the bluetooth class information. The returned
  // values are unique, and do not overlap, so DEVICE_KEYBOARD is not also
  // DEVICE_PERIPHERAL.
  DeviceType GetDeviceType() const;

  // Returns a localized string containing the device's bluetooth address and
  // a device type for display when |name_| is empty.
  string16 GetAddressWithLocalizedDeviceTypeName() const;

  // Indicates whether the class of this device is supported by Chrome OS.
  bool IsSupported() const;

  // Indicates whether the device is paired to the adapter, whether or not
  // that pairing is permanent or temporary.
  virtual bool IsPaired() const;

  // Indicates whether the device is visible to the adapter, this is not
  // mutually exclusive to being paired.
  bool IsVisible() const { return visible_; }

  // Indicates whether the device is bonded to the adapter, bonding is
  // formed by pairing and exchanging high-security link keys so that
  // connections may be encrypted.
  virtual bool IsBonded() const;

  // Indicates whether the device is currently connected to the adapter
  // and at least one service available for use.
  virtual bool IsConnected() const;

  // Returns the services (as UUID strings) that this device provides.
  typedef std::vector<std::string> ServiceList;
  const ServiceList& GetServices() const { return service_uuids_; }

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  typedef base::Callback<void()> ErrorCallback;

  // Returns the services (as BluetoothServiceRecord objects) that this device
  // provides.
  typedef ScopedVector<BluetoothServiceRecord> ServiceRecordList;
  typedef base::Callback<void(const ServiceRecordList&)> ServiceRecordsCallback;
  void GetServiceRecords(const ServiceRecordsCallback& callback,
                         const ErrorCallback& error_callback);

  // Indicates whether this device provides the given service.  |uuid| should
  // be in canonical form (see bluetooth_utils::CanonicalUuid).
  virtual bool ProvidesServiceWithUUID(const std::string& uuid) const;

  // The ProvidesServiceCallback is used by ProvidesServiceWithName to indicate
  // whether or not a matching service was found.
  typedef base::Callback<void(bool)> ProvidesServiceCallback;

  // Indicates whether this device provides the given service.
  virtual void ProvidesServiceWithName(const std::string& name,
                                       const ProvidesServiceCallback& callback);

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  bool ExpectingPinCode() const { return !pincode_callback_.is_null(); }

  // Indicates whether the device is currently pairing and expecting a
  // Passkey to be returned.
  bool ExpectingPasskey() const { return !passkey_callback_.is_null(); }

  // Indicates whether the device is currently pairing and expecting
  // confirmation of a displayed passkey.
  bool ExpectingConfirmation() const {
    return !confirmation_callback_.is_null();
  }

  // Initiates a connection to the device, pairing first if necessary.
  //
  // Method calls will be made on the supplied object |pairing_delegate|
  // to indicate what display, and in response should make method calls
  // back to the device object. Not all devices require user responses
  // during pairing, so it is normal for |pairing_delegate| to receive no
  // calls. To explicitly force a low-security connection without bonding,
  // pass NULL, though this is ignored if the device is already paired.
  //
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  void Connect(PairingDelegate* pairing_delegate,
               const base::Closure& callback,
               const ErrorCallback& error_callback);

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

  // Rejects a pairing or connection request from a remote device.
  void RejectPairing();

  // Cancels a pairing or connection attempt to a remote device.
  void CancelPairing();

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it. Link keys and other pairing
  // information are not discarded, and the device object is not deleted.
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  void Disconnect(const base::Closure& callback,
                  const ErrorCallback& error_callback);

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it, and then discards link keys
  // and other pairing information. The device object remainds valid until
  // returing from the calling function, after which it should be assumed to
  // have been deleted. If the request fails, |error_callback| will be called.
  // There is no callback for success beause this object is often deleted
  // before that callback would be called.
  void Forget(const ErrorCallback& error_callback);

  // SocketCallback is used by ConnectToService to return a BluetoothSocket
  // to the caller, or NULL if there was an error.  The socket will remain open
  // until the last reference to the returned BluetoothSocket is released.
  typedef base::Callback<void(scoped_refptr<BluetoothSocket>)> SocketCallback;

  // Attempts to open a socket to a service matching |uuid| on this device.  If
  // the connection is successful, |callback| is called with a BluetoothSocket.
  // Otherwise |callback| is called with NULL.  The socket is closed as soon as
  // all references to the BluetoothSocket are released.  Note that the
  // BluetoothSocket object can outlive both this BluetoothDevice and the
  // BluetoothAdapter for this device.
  void ConnectToService(const std::string& service_uuid,
                        const SocketCallback& callback);

  // Sets the Out Of Band pairing data for this device to |data|.  Exactly one
  // of |callback| or |error_callback| will be run.
  virtual void SetOutOfBandPairingData(
      const chromeos::BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback);

  // Clears the Out Of Band pairing data for this device.  Exactly one of
  // |callback| or |error_callback| will be run.
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback);

 private:
  friend class BluetoothAdapter;
  friend class MockBluetoothDevice;

  explicit BluetoothDevice(BluetoothAdapter* adapter);

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
  void ConnectCallback(const base::Closure& callback,
                       const ErrorCallback& error_callback,
                       const dbus::ObjectPath& device_path);

  // Called by BluetoothAdapterClient when a call to CreateDevice() or
  // CreatePairedDevice() fails with the error named |error_name| and
  // optional message |error_message|, |error_callback| is the callback
  // provided to Connect().
  void ConnectErrorCallback(const ErrorCallback& error_callback,
                            const std::string& error_name,
                            const std::string& error_message);

  // Called by BluetoothAdapterClient when a call to DiscoverServices()
  // completes.  |callback| and |error_callback| are the callbacks provided to
  // GetServiceRecords.
  void CollectServiceRecordsCallback(
      const ServiceRecordsCallback& callback,
      const ErrorCallback& error_callback,
      const dbus::ObjectPath& device_path,
      const BluetoothDeviceClient::ServiceMap& service_map,
      bool success);

  // Called by BluetoothProperty when the call to Set() for the Trusted
  // property completes. |success| indicates whether or not the request
  // succeeded, |callback| and |error_callback| are the callbacks provided to
  // Connect().
  void OnSetTrusted(const base::Closure& callback,
                    const ErrorCallback& error_callback,
                    bool success);

  // Connect application-level protocols of the device to the system, called
  // on a successful connection or to reconnect to a device that is already
  // paired or previously connected. |error_callback| is called on failure.
  // Otherwise, |callback| is called when the request is complete.
  void ConnectApplications(const base::Closure& callback,
                           const ErrorCallback& error_callback);

  // Called by IntrospectableClient when a call to Introspect() completes.
  // |success| indicates whether or not the request succeeded, |callback| and
  // |error_callback| are the callbacks provided to ConnectApplications(),
  // |service_name| and |device_path| specify the remote object being
  // introspected and |xml_data| contains the XML-formatted protocol data.
  void OnIntrospect(const base::Closure& callback,
                    const ErrorCallback& error_callback,
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
  void OnConnectError(const ErrorCallback& error_callback,
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

  // Called by BluetoothAdapterClient when a call to RemoveDevice() completes,
  // |success| indicates whether or not the request succeeded, |error_callback|
  // is the callback provided to Forget() and |adapter_path| is the d-bus
  // object path of the adapter that performed the removal.
  void ForgetCallback(const ErrorCallback& error_callback,
                      const dbus::ObjectPath& adapter_path, bool success);

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

  // Creates a new BluetoothDevice object bound to the adapter |adapter|.
  static BluetoothDevice* Create(BluetoothAdapter* adapter);

  // The adapter that owns this device instance.
  BluetoothAdapter* adapter_;

  // The dbus object path of the device, will be empty if the device has only
  // been discovered and not yet paired with.
  dbus::ObjectPath object_path_;

  // The Bluetooth address of the device.
  std::string address_;

  // The name of the device, as supplied by the remote device.
  std::string name_;

  // The Bluetooth class of the device, a bitmask that may be decoded using
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32 bluetooth_class_;

  // Tracked device state, updated by the adapter managing the lifecyle of
  // the device.
  bool visible_;
  bool bonded_;
  bool connected_;

  // The services (identified by UUIDs) that this device provides.
  std::vector<std::string> service_uuids_;

  // During pairing this is set to an object that we don't own, but on which
  // we can make method calls to request, display or confirm PIN Codes and
  // Passkeys. Generally it is the object that owns this one.
  PairingDelegate* pairing_delegate_;

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

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDevice> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothDevice);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BLUETOOTH_BLUETOOTH_DEVICE_H_
