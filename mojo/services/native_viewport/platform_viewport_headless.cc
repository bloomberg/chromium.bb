// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/platform_viewport_headless.h"

namespace mojo {

PlatformViewportHeadless::PlatformViewportHeadless(Delegate* delegate)
    : delegate_(delegate) {
}

PlatformViewportHeadless::~PlatformViewportHeadless() {
}

void PlatformViewportHeadless::Init(const gfx::Rect& bounds) {
  bounds_ = bounds;
}

void PlatformViewportHeadless::Show() {
}

void PlatformViewportHeadless::Hide() {
}

void PlatformViewportHeadless::Close() {
  delegate_->OnDestroyed();
}

gfx::Size PlatformViewportHeadless::GetSize() {
  return bounds_.size();
}

void PlatformViewportHeadless::SetBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds_);
}

void PlatformViewportHeadless::SetCapture() {
}

void PlatformViewportHeadless::ReleaseCapture() {
}

// static
scoped_ptr<PlatformViewport> PlatformViewportHeadless::Create(
    Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(
      new PlatformViewportHeadless(delegate)).Pass();
}

}  // namespace mojo
