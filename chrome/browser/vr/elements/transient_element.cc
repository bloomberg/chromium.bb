// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transient_element.h"

namespace vr {

TransientElement::TransientElement(const base::TimeDelta& timeout)
    : timeout_(timeout) {
  SetVisibleImmediately(false);
}

TransientElement::~TransientElement() {}

void TransientElement::SetVisible(bool visible) {
  // We're already at the desired visibility, no-op.
  if (visible == (GetTargetOpacity() == opacity_when_visible()))
    return;

  if (visible)
    set_visible_time_ = last_frame_time();

  super::SetVisible(visible);
}

void TransientElement::RefreshVisible() {
  // Do nothing if we're not going to be visible.
  if (GetTargetOpacity() != opacity_when_visible())
    return;

  set_visible_time_ = last_frame_time();
}

SimpleTransientElement::SimpleTransientElement(const base::TimeDelta& timeout)
    : super(timeout) {}

SimpleTransientElement::~SimpleTransientElement() {}

bool SimpleTransientElement::OnBeginFrame(
    const base::TimeTicks& time,
    const gfx::Vector3dF& head_direction) {
  // Do nothing if we're not going to be visible.
  if (GetTargetOpacity() != opacity_when_visible())
    return false;

  // SetVisible may have been called during initialization which means that the
  // last frame time would be zero.
  if (set_visible_time_.is_null())
    set_visible_time_ = last_frame_time();

  base::TimeDelta duration = time - set_visible_time_;
  if (duration >= timeout_) {
    super::SetVisible(false);
    return true;
  }
  return false;
}

ShowUntilSignalTransientElement::ShowUntilSignalTransientElement(
    const base::TimeDelta& min_duration,
    const base::TimeDelta& timeout,
    const base::Callback<void(TransientElementHideReason)>& callback)
    : super(timeout), min_duration_(min_duration), callback_(callback) {
  SetVisibleImmediately(false);
}

ShowUntilSignalTransientElement::~ShowUntilSignalTransientElement() {}

bool ShowUntilSignalTransientElement::OnBeginFrame(
    const base::TimeTicks& time,
    const gfx::Vector3dF& head_direction) {
  // Do nothing if we're not going to be visible.
  if (GetTargetOpacity() != opacity_when_visible())
    return false;

  // SetVisible may have been called during initialization which means that the
  // last frame time would be zero.
  if (set_visible_time_.is_null())
    set_visible_time_ = last_frame_time();

  bool set_invisible = false;

  base::TimeDelta duration = time - set_visible_time_;
  if (duration > timeout_) {
    callback_.Run(TransientElementHideReason::kTimeout);
    set_invisible = true;
  } else if (duration >= min_duration_ && signaled_) {
    callback_.Run(TransientElementHideReason::kSignal);
    set_invisible = true;
  }
  if (set_invisible) {
    super::SetVisible(false);
    return true;
  }
  return false;
}

void ShowUntilSignalTransientElement::Signal() {
  signaled_ = true;
}

}  // namespace vr
