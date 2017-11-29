// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/accelerometer/accelerometer_types.h"

namespace chromeos {

AccelerometerReading::AccelerometerReading() : present(false) {
}

AccelerometerReading::~AccelerometerReading() = default;

AccelerometerUpdate::AccelerometerUpdate() = default;

AccelerometerUpdate::~AccelerometerUpdate() = default;

}  // namespace chromeos
