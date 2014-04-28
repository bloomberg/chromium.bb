// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_MAC_H_

#import <IOBluetooth/IOBluetooth.h>

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/mac/scoped_nsobject.h"
#include "base/sequenced_task_runner.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_socket.h"
#include "device/bluetooth/bluetooth_uuid.h"

@class BluetoothProfileMacHelper;
@class IOBluetoothDevice;
@class IOBluetoothRFCOMMChannel;

namespace device {

class BluetoothProfileMac : public BluetoothProfile {
 public:
  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  // Makes an outgoing connection to |device|, calling |success_callback| on
  // success or |error_callback| on error. If successful, this method also calls
  // |connection_callback_| before calling |success_callback|.
  void Connect(IOBluetoothDevice* device,
               const base::Closure& success_callback,
               const BluetoothSocket::ErrorCompletionCallback& error_callback);

  // Callback that is invoked when the OS detects a new incoming RFCOMM channel
  // connection. |rfcomm_channel| is the newly opened channel.
  void OnRFCOMMChannelOpened(IOBluetoothRFCOMMChannel* rfcomm_channel);

 private:
  friend BluetoothProfile* CreateBluetoothProfileMac(const BluetoothUUID& uuid,
                                                     const Options& options);

  BluetoothProfileMac(const BluetoothUUID& uuid, const Options& options);
  virtual ~BluetoothProfileMac();

  const BluetoothUUID uuid_;
  const uint16 rfcomm_channel_id_;
  ConnectionCallback connection_callback_;

  // A simple helper that registers for OS notifications and forwards them to
  // |this| profile.
  base::scoped_nsobject<BluetoothProfileMacHelper> helper_;

  // A handle to the service record registered in the system SDP server.
  // Used to eventually unregister the service.
  const BluetoothSDPServiceRecordHandle service_record_handle_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_MAC_H_
