// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_CTAP_DEVICE_COMMAND_H_
#define DEVICE_CTAP_CTAP_DEVICE_COMMAND_H_

#include <stdint.h>

namespace device {

// Commands supported by CTAPHID device as specified in
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#ctaphid-commands
enum class CTAPHIDDeviceCommand : uint8_t {
  kCtapHidMsg = 0x03,
  kCtapHidCBOR = 0x10,
  kCtapHidInit = 0x06,
  kCtapHidPing = 0x01,
  kCtapHidCancel = 0x11,
  kCtapHidError = 0x3F,
  kCtapHidKeepAlive = 0x3B,
  kCtapHidWink = 0x08,
  kCtapHidLock = 0x04,
};

}  // namespace device

#endif  // DEVICE_CTAP_CTAP_DEVICE_COMMAND_H_
