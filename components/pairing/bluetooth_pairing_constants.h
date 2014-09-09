// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PAIRING_BLUETOOTH_PAIRING_CONSTANTS_H_
#define CHROMEOS_PAIRING_BLUETOOTH_PAIRING_CONSTANTS_H_

namespace pairing_chromeos {

extern const char* kPairingServiceUUID;
extern const char* kPairingServiceName;
extern const char* kDeviceNamePrefix;
extern const char* kErrorInvalidProtocol;
extern const char* kErrorEnrollmentFailed;
extern const int kPairingAPIVersion;

enum {
  PAIRING_ERROR_NONE = 0,
  PAIRING_ERROR_PAIRING_OR_ENROLLMENT = 1,
};

}  // namespace pairing_chromeos

#endif  // CHROMEOS_PAIRING_BLUETOOTH_PAIRING_CONSTANTS_H_
