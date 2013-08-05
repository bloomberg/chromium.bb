// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/orientation.h"

#include <cmath>

#include "content/common/device_orientation/device_orientation_messages.h"

namespace content {

Orientation::Orientation()
    : can_provide_alpha_(false),
      can_provide_beta_(false),
      can_provide_gamma_(false),
      can_provide_absolute_(false) {
}

Orientation::~Orientation() {
}

IPC::Message* Orientation::CreateIPCMessage(int render_view_id) const {
  DeviceOrientationMsg_Updated_Params params;
  params.can_provide_alpha = can_provide_alpha_;
  params.alpha = alpha_;
  params.can_provide_beta = can_provide_beta_;
  params.beta = beta_;
  params.can_provide_gamma = can_provide_gamma_;
  params.gamma = gamma_;
  params.can_provide_absolute = can_provide_absolute_;
  params.absolute = absolute_;

  return new DeviceOrientationMsg_Updated(render_view_id, params);
}

// Returns true if two orientations are considered different enough that
// observers should be notified of the new orientation.
bool Orientation::ShouldFireEvent(const DeviceData* old_data) const {
  scoped_refptr<const Orientation> old_orientation(
      static_cast<const Orientation*>(old_data));

  return IsElementSignificantlyDifferent(can_provide_alpha_,
                                         old_orientation->can_provide_alpha(),
                                         alpha_,
                                         old_orientation->alpha()) ||
        IsElementSignificantlyDifferent(can_provide_beta_,
                                        old_orientation->can_provide_beta(),
                                        beta_,
                                        old_orientation->beta()) ||
        IsElementSignificantlyDifferent(can_provide_gamma_,
                                        old_orientation->can_provide_gamma(),
                                        gamma_,
                                        old_orientation->gamma()) ||
        can_provide_absolute_ != old_orientation->can_provide_absolute() ||
        absolute_ != old_orientation->absolute();
}

bool Orientation::IsElementSignificantlyDifferent(bool can_provide_element1,
    bool can_provide_element2, double element1, double element2) {
  const double kThreshold = 0.1;

  if (can_provide_element1 != can_provide_element2)
    return true;
  if (can_provide_element1 && std::fabs(element1 - element2) >= kThreshold)
    return true;
  return false;
}

}  // namespace content
