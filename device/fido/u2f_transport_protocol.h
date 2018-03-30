// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_TRANSPORT_PROTOCOL_H_
#define DEVICE_U2F_U2F_TRANSPORT_PROTOCOL_H_

namespace device {

// This enum represents the transport protocols over which U2F is currently
// supported.
enum class U2fTransportProtocol {
  kUsbHumanInterfaceDevice,
  kNearFieldCommunication,
  kBluetoothLowEnergy,
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_TRANSPORT_PROTOCOL_H_
