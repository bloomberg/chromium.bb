// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ozone/platform_window_cast.h"

#include "ui/platform_window/platform_window_delegate.h"

namespace chromecast {
namespace ozone {

PlatformWindowCast::PlatformWindowCast(ui::PlatformWindowDelegate* delegate,
                                       const gfx::Rect& bounds)
    : delegate_(delegate), bounds_(bounds) {
  widget_ = (bounds.width() << 16) + bounds.height();
  delegate_->OnAcceleratedWidgetAvailable(widget_);
}

gfx::Rect PlatformWindowCast::GetBounds() {
  return bounds_;
}

void PlatformWindowCast::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

}  // namespace ozone
}  // namespace chromecast
