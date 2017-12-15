// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_BLE_UUIDS_H_
#define DEVICE_U2F_U2F_BLE_UUIDS_H_

namespace device {

// U2F GATT Service's UUIDs as defined by the standard:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-bt-protocol-v1.2-ps-20170411.html#h3_u2f-service
//
// For details on how the short UUIDs for U2F Service (0xFFFD) and U2F Service
// Revision (0x2A28) were converted to the long canonical ones, see
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
extern const char kU2fServiceUUID[];
extern const char kU2fControlPointUUID[];
extern const char kU2fStatusUUID[];
extern const char kU2fControlPointLengthUUID[];
extern const char kU2fServiceRevisionUUID[];
extern const char kU2fServiceRevisionBitfieldUUID[];

}  // namespace device

#endif  // DEVICE_U2F_U2F_BLE_UUIDS_H_
