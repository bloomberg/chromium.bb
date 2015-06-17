// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/native_viewport/platform_viewport_headless.h"

#include "mojo/converters/geometry/geometry_type_converters.h"

namespace native_viewport {

PlatformViewportHeadless::PlatformViewportHeadless(Delegate* delegate)
    : delegate_(delegate) {
}

PlatformViewportHeadless::~PlatformViewportHeadless() {
}

void PlatformViewportHeadless::Init(const gfx::Rect& bounds) {
  metrics_ = mojo::ViewportMetrics::New();
  metrics_->device_pixel_ratio = 1.f;
  metrics_->size_in_pixels = mojo::Size::From(bounds.size());

  // The delegate assumes an initial metrics of 0.
  delegate_->OnMetricsChanged(bounds.size(), 1.f /* device_scale_factor */);
}

void PlatformViewportHeadless::Show() {
}

void PlatformViewportHeadless::Hide() {
}

void PlatformViewportHeadless::Close() {
  delegate_->OnDestroyed();
}

gfx::Size PlatformViewportHeadless::GetSize() {
  return metrics_->size_in_pixels.To<gfx::Size>();
}

void PlatformViewportHeadless::SetBounds(const gfx::Rect& bounds) {
  delegate_->OnMetricsChanged(bounds.size(), 1.f /* device_scale_factor */);
}

// static
scoped_ptr<PlatformViewport> PlatformViewportHeadless::Create(
    Delegate* delegate) {
  return scoped_ptr<PlatformViewport>(
      new PlatformViewportHeadless(delegate)).Pass();
}

}  // namespace native_viewport
