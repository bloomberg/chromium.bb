// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/multidevice/software_feature_state.h"

namespace chromeos {

namespace multidevice {

std::ostream& operator<<(std::ostream& stream,
                         const SoftwareFeatureState& state) {
  switch (state) {
    case SoftwareFeatureState::kNotSupported:
      stream << "[not supported]";
      break;
    case SoftwareFeatureState::kSupported:
      stream << "[supported]";
      break;
    case SoftwareFeatureState::kEnabled:
      stream << "[enabled]";
      break;
  }
  return stream;
}

}  // namespace multidevice

}  // namespace chromeos
