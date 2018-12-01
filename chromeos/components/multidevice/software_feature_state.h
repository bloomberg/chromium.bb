// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_STATE_H_
#define CHROMEOS_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_STATE_H_

#include <ostream>

namespace chromeos {

namespace multidevice {

enum class SoftwareFeatureState {
  kNotSupported = 0,
  kSupported = 1,
  kEnabled = 2
};

std::ostream& operator<<(std::ostream& stream,
                         const SoftwareFeatureState& state);

}  // namespace multidevice

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_MULTIDEVICE_SOFTWARE_FEATURE_STATE_H_
