// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/string16.h"
#include "base/memory/ref_counted.h"

namespace device {

class BluetoothServiceRecord;
class BluetoothSocket;

struct BluetoothOutOfBandPairingData;

// BluetoothDevice represents a remote Bluetooth device, both its properties and
// capabilities as discovered by a local adapter and actions that may be
// performed on the remove device such as pairing, connection and disconnection.
//
// The class is instantiated and managed by the BluetoothAdapter class
// and pointers should only be obtained from that class and not cached,
// instead use the address() method as a unique key for a device.
//
// Since the lifecycle of BluetoothDevice instances is managed by
// BluetoothAdapter, that class rather than this provides observer methods
// for devices coming and going, as well as properties being updated.
class BluetoothDevice {
 public:
  // Possible values that may be returned by GetDeviceType(), representing
  // different types of bluetooth device that we support or are aware of
  // decoded from the bluetooth class information.
  enum DeviceType {
    DEVICE_UNKNOWN,
    DEVICE_COMPUTER,
    DEVICE_PHONE,
    DEVICE_MODEM,
    DEVICE_AUDIO,
    DEVICE_CAR_AUDIO,
    DEVICE_VIDEO,
    DEVICE_PERIPHERAL,
    DEVICE_JOYSTICK,
    DEVICE_GAMEPAD,
    DEVICE_KEYBOARD,
    DEVICE_MOUSE,
    DEVICE_TABLET,
    DEVICE_KEYBOARD_MOUSE_COMBO
  };

  // Possible errors passed back to an error callback function in case of a
  // failed call to Connect().
  enum ConnectErrorCode {
    ERROR_UNKNOWN,
    ERROR_INPROGRESS,
    ERROR_FAILED,
    ERROR_AUTH_FAILED,
    ERROR_AUTH_CANCELED,
    ERROR_AUTH_REJECTED,
    ERROR_AUTH_TIMEOUT,
    ERROR_UNSUPPORTED_DEVICE
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

  // Indicates whether the device is paired to the adapter, whether or not
  // that pairing is permanent or temporary.
  virtual bool IsPaired() const = 0;

  // Indicates whether the device is visible to the adapter, this is not
  // mutually exclusive to being paired.
  virtual bool IsVisible() const;

  // Indicates whether the device is bonded to the adapter, bonding is
  // formed by pairing and exchanging high-security link keys so that
  // connections may be encrypted.
  virtual bool IsBonded() const;

  // Indicates whether the device is currently connected to the adapter
  // and at least one service available for use.
  virtual bool IsConnected() const;

  // Returns the services (as UUID strings) that this device provides.
  typedef std::vector<std::string> ServiceList;
  virtual const ServiceList& GetServices() const = 0;

  // The ErrorCallback is used for methods that can fail in which case it
  // is called, in the success case the callback is simply not called.
  typedef base::Callback<void()> ErrorCallback;

  // The ConnectErrorCallback is used for methods that can fail with an error,
  // passed back as an error code argument to this callback.
  // In the success case this callback is not called.
  typedef base::Callback<void(enum ConnectErrorCode)> ConnectErrorCallback;

  // Returns the services (as BluetoothServiceRecord objects) that this device
  // provides.
  typedef ScopedVector<BluetoothServiceRecord> ServiceRecordList;
  typedef base::Callback<void(const ServiceRecordList&)> ServiceRecordsCallback;
  virtual void GetServiceRecords(const ServiceRecordsCallback& callback,
                                 const ErrorCallback& error_callback) = 0;

  // Indicates whether this device provides the given service.  |uuid| should
  // be in canonical form (see utils::CanonicalUuid).
  virtual bool ProvidesServiceWithUUID(const std::string& uuid) const = 0;

  // The ProvidesServiceCallback is used by ProvidesServiceWithName to indicate
  // whether or not a matching service was found.
  typedef base::Callback<void(bool)> ProvidesServiceCallback;

  // Indicates whether this device provides the given service.
  virtual void ProvidesServiceWithName(
      const std::string& name,
      const ProvidesServiceCallback& callback) = 0;

  // Indicates whether the device is currently pairing and expecting a
  // PIN Code to be returned.
  virtual bool ExpectingPinCode() const = 0;

  // Indicates whether the device is currently pairing and expecting a
  // Passkey to be returned.
  virtual bool ExpectingPasskey() const = 0;

  // Indicates whether the device is currently pairing and expecting
  // confirmation of a displayed passkey.
  virtual bool ExpectingConfirmation() const = 0;

  // SocketCallback is used by ConnectToService to return a BluetoothSocket to
  // the caller, or NULL if there was an error.  The socket will remain open
  // until the last reference to the returned BluetoothSocket is released.
  typedef base::Callback<void(scoped_refptr<BluetoothSocket>)>
      SocketCallback;

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
  // After calling Connect, CancelPairing should be called to cancel the pairing
  // process and release |pairing_delegate_| if user cancels the pairing and
  // closes the pairing UI.
  virtual void Connect(PairingDelegate* pairing_delegate,
                       const base::Closure& callback,
                       const ConnectErrorCallback& error_callback) = 0;

  // Sends the PIN code |pincode| to the remote device during pairing.
  //
  // PIN Codes are generally required for Bluetooth 2.0 and earlier devices
  // for which there is no automatic pairing or special handling.
  virtual void SetPinCode(const std::string& pincode) = 0;

  // Sends the Passkey |passkey| to the remote device during pairing.
  //
  // Passkeys are generally required for Bluetooth 2.1 and later devices
  // which cannot provide input or display on their own, and don't accept
  // passkey-less pairing, and are a numeric in the range 0-999999.
  virtual void SetPasskey(uint32 passkey) = 0;

  // Confirms to the remote device during pairing that a passkey provided by
  // the ConfirmPasskey() delegate call is displayed on both devices.
  virtual void ConfirmPairing() = 0;

  // Rejects a pairing or connection request from a remote device.
  virtual void RejectPairing() = 0;

  // Cancels a pairing or connection attempt to a remote device or release
  // |pairing_deleage_| and |agent_|.
  virtual void CancelPairing() = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it. Link keys and other pairing
  // information are not discarded, and the device object is not deleted.
  // If the request fails, |error_callback| will be called; otherwise,
  // |callback| is called when the request is complete.
  virtual void Disconnect(const base::Closure& callback,
                          const ErrorCallback& error_callback) = 0;

  // Disconnects the device, terminating the low-level ACL connection
  // and any application connections using it, and then discards link keys
  // and other pairing information. The device object remainds valid until
  // returing from the calling function, after which it should be assumed to
  // have been deleted. If the request fails, |error_callback| will be called.
  // There is no callback for success beause this object is often deleted
  // before that callback would be called.
  virtual void Forget(const ErrorCallback& error_callback) = 0;

  // Attempts to open a socket to a service matching |uuid| on this device.  If
  // the connection is successful, |callback| is called with a BluetoothSocket.
  // Otherwise |callback| is called with NULL.  The socket is closed as soon as
  // all references to the BluetoothSocket are released.  Note that the
  // BluetoothSocket object can outlive both this BluetoothDevice and the
  // BluetoothAdapter for this device.
  virtual void ConnectToService(const std::string& service_uuid,
                                const SocketCallback& callback) = 0;

  // Sets the Out Of Band pairing data for this device to |data|.  Exactly one
  // of |callback| or |error_callback| will be run.
  virtual void SetOutOfBandPairingData(
      const BluetoothOutOfBandPairingData& data,
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

  // Clears the Out Of Band pairing data for this device.  Exactly one of
  // |callback| or |error_callback| will be run.
  virtual void ClearOutOfBandPairingData(
      const base::Closure& callback,
      const ErrorCallback& error_callback) = 0;

 protected:
  BluetoothDevice();

  // The Bluetooth class of the device, a bitmask that may be decoded using
  // https://www.bluetooth.org/Technical/AssignedNumbers/baseband.htm
  uint32 bluetooth_class_;

  // The name of the device, as supplied by the remote device.
  std::string name_;

  // The Bluetooth address of the device.
  std::string address_;

  // Tracked device state, updated by the adapter managing the lifecyle of
  // the device.
  bool visible_;
  bool bonded_;
  bool connected_;

 private:
  // Returns a localized string containing the device's bluetooth address and
  // a device type for display when |name_| is empty.
  string16 GetAddressWithLocalizedDeviceTypeName() const;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_DEVICE_H_
