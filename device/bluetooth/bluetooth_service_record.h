// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_H_

#include <string>

#include "base/basictypes.h"

namespace device {

// BluetoothServiceRecord represents an SDP service record.
//
// This implementation is currently incomplete: it only supports those fields
// that have been necessary so far.
class BluetoothServiceRecord {
 public:
  virtual ~BluetoothServiceRecord();

  // The human-readable name of this service.
  const std::string& name() const { return name_; }

  // The address of the BluetoothDevice providing this service.
  const std::string& address() const { return address_; }

  // The UUID of the service.  This field may be empty if no UUID was
  // specified in the service record.
  const std::string& uuid() const { return uuid_; }

  // Indicates if this service supports RFCOMM communication.
  bool SupportsRfcomm() const { return supports_rfcomm_; }

  // The RFCOMM channel to use, if this service supports RFCOMM communication.
  // The return value is undefined if SupportsRfcomm() returns false.
  uint8 rfcomm_channel() const { return rfcomm_channel_; }

 protected:
  BluetoothServiceRecord();

  std::string address_;
  std::string name_;
  std::string uuid_;

  bool supports_rfcomm_;
  uint8 rfcomm_channel_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BluetoothServiceRecord);
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_SERVICE_RECORD_H_
