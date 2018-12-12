// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_DEVICE_CLASSIFIER_UTIL_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_DEVICE_CLASSIFIER_UTIL_H_

#include "chromeos/services/device_sync/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace device_sync {

namespace device_classifier_util {

const cryptauth::DeviceClassifier& GetDeviceClassifier();

}  // namespace device_classifier_util

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_PROTO_DEVICE_CLASSIFIER_UTIL_H_
