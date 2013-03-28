// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_VOLUME_STATE_H_
#define CHROMEOS_DBUS_VOLUME_STATE_H_

#include <string>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

struct CHROMEOS_EXPORT VolumeState {
  int32 output_volume;
  bool output_mute;
  int32 input_gain;
  bool input_mute;

  VolumeState();
  std::string ToString() const;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_VOLUME_STATE_H_
