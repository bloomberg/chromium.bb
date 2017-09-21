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

void TransientElement::OnBeginFrame(const base::TimeTicks& time) {
  super::OnBeginFrame(time);

  // Do nothing if we're not going to be visible.
  if (!(GetTargetOpacity() == opacity_when_visible()))
    return;

  // SetVisible may have been called during initialization which means that the
  // last frame time would be zero.
  if (set_visible_time_.is_null())
    set_visible_time_ = last_frame_time();

  base::TimeDelta duration = time - set_visible_time_;
  if (duration >= timeout_)
    super::SetVisible(false);
}

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
  if (!(GetTargetOpacity() == opacity_when_visible()))
    return;

  set_visible_time_ = last_frame_time();
}

}  // namespace vr
