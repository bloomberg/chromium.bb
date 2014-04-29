// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_profile.h"
#include "device/bluetooth/bluetooth_uuid.h"
#include "net/base/net_log.h"
#include "net/socket/tcp_socket.h"

namespace net {
class IPEndPoint;
}

namespace device {

class BluetoothAdapter;
class BluetoothAdapterWin;
class BluetoothDeviceWin;
class BluetoothSocketThreadWin;
class BluetoothSocketWin;

class BluetoothProfileWin : public BluetoothProfile {
 public:
  typedef base::Callback<void(const std::string&)> ErrorCallback;

  // BluetoothProfile override.
  virtual void Unregister() OVERRIDE;
  virtual void SetConnectionCallback(
      const ConnectionCallback& callback) OVERRIDE;

  // Called by BluetoothProfile::Register to initialize the profile object
  // asynchronously.
  void Init(const BluetoothUUID& uuid,
            const device::BluetoothProfile::Options& options,
            const ProfileCallback& callback);

  void Connect(const BluetoothDeviceWin* device,
               scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
               scoped_refptr<BluetoothSocketThreadWin> socket_thread,
               net::NetLog* net_log,
               const net::NetLog::Source& source,
               const base::Closure& callback,
               const ErrorCallback& error_callback);

 private:
  friend BluetoothProfile;

  BluetoothProfileWin();
  virtual ~BluetoothProfileWin();

  // Internal method run to get the adapter object during initialization.
  void OnGetAdapter(const ProfileCallback& callback,
                    scoped_refptr<BluetoothAdapter> adapter);

  // Callbacks for |profile_socket_|'s StartService call.
  void OnRegisterProfileSuccess(const ProfileCallback& callback);
  void OnRegisterProfileError(const ProfileCallback& callback,
                              const std::string& error_message);

  // Callback when |profile_socket_| accepts a connection.
  void OnNewConnection(scoped_refptr<BluetoothSocketWin> connected,
                       const net::IPEndPoint& peer_address);

  BluetoothAdapterWin* adapter() const;

  BluetoothUUID uuid_;
  std::string name_;
  int rfcomm_channel_;
  ConnectionCallback connection_callback_;

  scoped_refptr<BluetoothAdapter> adapter_;
  scoped_refptr<BluetoothSocketWin> profile_socket_;
  base::WeakPtrFactory<BluetoothProfileWin> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothProfileWin);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_PROFILE_WIN_H_
