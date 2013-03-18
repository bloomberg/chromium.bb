// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "device/bluetooth/bluetooth_socket.h"

#ifdef __OBJC__
@class BluetoothRFCOMMChannelDelegate;
@class IOBluetoothRFCOMMChannel;
#else
class BluetoothRFCOMMChannelDelegate;
class IOBluetoothRFCOMMChannel;
#endif

namespace net {

class DrainableIOBuffer;
class GrowableIOBuffer;

}  // namespace net

namespace device {

class BluetoothServiceRecord;

// This class is an implementation of BluetoothSocket class for OSX platform.
class BluetoothSocketMac : public BluetoothSocket {
 public:
  static scoped_refptr<BluetoothSocket> CreateBluetoothSocket(
      const BluetoothServiceRecord& service_record);

  // BluetoothSocket override
  virtual bool Receive(net::GrowableIOBuffer* buffer) OVERRIDE;
  virtual bool Send(net::DrainableIOBuffer* buffer) OVERRIDE;
  virtual std::string GetLastErrorMessage() const OVERRIDE;

  // called by BluetoothRFCOMMChannelDelegate.
  void OnDataReceived(IOBluetoothRFCOMMChannel* rfcomm_channel,
                      void* data,
                      size_t length);

 protected:
  virtual ~BluetoothSocketMac();

 private:
  explicit BluetoothSocketMac(IOBluetoothRFCOMMChannel* rfcomm_channel);

  void ResetIncomingDataBuffer();

  IOBluetoothRFCOMMChannel* rfcomm_channel_;
  BluetoothRFCOMMChannelDelegate* delegate_;
  scoped_refptr<net::GrowableIOBuffer> incoming_data_buffer_;
  std::string error_message_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothSocketMac);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SOCKET_MAC_H_
