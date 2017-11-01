// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_BLE_UUIDS_H_
#define DEVICE_U2F_U2F_BLE_UUIDS_H_

namespace device {

// U2F GATT Service's UUIDs as defined by the standard:
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-bt-protocol-v1.2-ps-20170411.html#h3_u2f-service
//
// For details on how the short U2F Service UUID (0xfffd) was converted to the
// long one below, see
// https://www.bluetooth.com/specifications/assigned-numbers/service-discovery
static constexpr const char U2F_SERVICE_UUID[] =
    "0000fffd-0000-1000-8000-00805f9b34fb";
static constexpr const char U2F_CONTROL_POINT_UUID[] =
    "f1d0fff1-deaa-ecee-b42f-c9ba7ed623bb";
static constexpr const char U2F_STATUS_UUID[] =
    "f1d0fff2-deaa-ecee-b42f-c9ba7ed623bb";
static constexpr const char U2F_CONTROL_POINT_LENGTH_UUID[] =
    "f1d0fff3-deaa-ecee-b42f-c9ba7ed623bb";

}  // namespace device

#endif  // DEVICE_U2F_U2F_BLE_UUIDS_H_
