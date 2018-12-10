// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_H_

#include <ostream>

#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace multidevice {

// Multi-device features. In this context, "host" refers to the device
// (typically an Android phone) which provides functionality, and "client"
// refers to the device (typically a Chromebook) which receives functionality.
//
// Note that numerical enum values must not be changed as these values are
// serialized to numbers and stored persistently.
//
// This enum should always be preferred over the cryptauth::SoftwareFeature
// proto except when communicating with the CryptAuth server.
enum class SoftwareFeature {
  // Note: Enum value 0 is intentionally skipped here for legacy reasons.

  // Support for multi-device features in general.
  kBetterTogetherHost = 1,
  kBetterTogetherClient = 2,

  // Smart Lock, which gives the user the ability to unlock and/or sign into a
  // Chromebook using an Android phone.
  kSmartLockHost = 3,
  kSmartLockClient = 4,

  // Instant Tethering, which gives the user the ability to use an Android
  // phone's Internet connection on a Chromebook.
  kInstantTetheringHost = 5,
  kInstantTetheringClient = 6,

  // Messages for Web, which gives the user the ability to sync messages (e.g.,
  // SMS) between an Android phone and a Chromebook.
  kMessagesForWebHost = 7,
  kMessagesForWebClient = 8
};

SoftwareFeature FromCryptAuthFeature(
    cryptauth::SoftwareFeature cryptauth_feature);
cryptauth::SoftwareFeature ToCryptAuthFeature(
    SoftwareFeature multidevice_feature);

std::ostream& operator<<(std::ostream& stream, const SoftwareFeature& feature);

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_H_
