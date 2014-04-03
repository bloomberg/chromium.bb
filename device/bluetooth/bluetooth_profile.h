// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_uuid.h"

namespace device {

class BluetoothDevice;
class BluetoothProfileMac;
class BluetoothSocket;
class MockBluetoothProfile;

// BluetoothProfile represents an implementation of either a client or server
// of a particular specified profile (aka service or protocol in other
// standards).
//
// Profile implementations are created by registering them through the static
// BluetoothProfile::Register() method and are always identified by a UUID
// which in any method may be specified in the short or long form.
//
// The lifecycle of BluetoothProfile instances is managed by the implementation
// but they are guaranteed to exist once provided to a Register() callback until
// the instance's Unregister() method is called, so others may hold on to
// pointers to them.
class BluetoothProfile {
 public:
  // Options used to register a BluetoothProfile object.
  struct Options {
    Options();
    ~Options();

    // Human readable name of the Profile, e.g. "Health Device".
    // Exported in the adapter's SDP or GATT tables where relevant.
    std::string name;

    // RFCOMM channel used by the profile.
    // Exported in the adapter's SDP or GATT tables where relevant.
    uint16 channel;

    // L2CAP PSM number.
    // Exported in the adapter's SDP or GATT tables where relevant.
    uint16 psm;

    // Specifies whether pairing (and encryption) is required to be able to
    // connect. Defaults to false.
    bool require_authentication;

    // Specifies whether user authorization is required to be able to connect.
    // Defaults to false.
    bool require_authorization;

    // Specifies whether this profile will be automatically connected if any
    // other profile of a device conects to the host.
    // Defaults to false.
    bool auto_connect;

    // Implemented version of the profile.
    // Exported in the adapter's SDP or GATT tables where relevant.
    uint16 version;

    // Implemented feature set of the profile.
    // Exported in the adapter's SDP or GATT tables where relevant.
    uint16 features;
  };

  // Register an implementation of the profile with UUID |uuid| and
  // additional details specified in |options|. The corresponding profile
  // object will be created and returned by |callback|. If the profile cannot
  // be registered, NULL will be passed.
  //
  // This pointer is not owned by the receiver, but will not be freed unless
  // its Unregister() method is called.
  typedef base::Callback<void(BluetoothProfile*)> ProfileCallback;
  static void Register(const BluetoothUUID& uuid,
                       const Options& options,
                       const ProfileCallback& callback);

  // Unregister the profile. This deletes the profile object so, once called,
  // any pointers to the profile should be discarded.
  virtual void Unregister() = 0;

  // Set the connection callback for the profile to |callback|, any successful
  // connection initiated by BluetoothDevice::ConnectToProfile() or incoming
  // connections from devices, will have a BluetoothSocket created and passed
  // to this callback.
  //
  // The socket will be closed when all references are released; none of the
  // BluetoothProfile, or BluetoothAdapter or BluetoothDevice objects are
  // guaranteed to hold a reference so this may outlive all of them.
  typedef base::Callback<void(
      const BluetoothDevice*,
      scoped_refptr<BluetoothSocket>)> ConnectionCallback;
  virtual void SetConnectionCallback(const ConnectionCallback& callback) = 0;

 protected:
  BluetoothProfile();
  virtual ~BluetoothProfile();
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_H_
