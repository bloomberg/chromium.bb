// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_root_window_transformer.h"

#include "ui/aura/root_window.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/size_conversions.h"

namespace ash {

AshRootWindowTransformer::AshRootWindowTransformer(
    aura::RootWindow* root,
    const gfx::Transform& transform,
    const gfx::Transform& inverted,
    const gfx::Insets& host_insets,
    float root_window_scale)
    : root_window_(root),
      transform_(transform),
      root_window_scale_(root_window_scale),
      host_insets_(host_insets) {
  root_window_->layer()->SetForceRenderSurface(root_window_scale_ != 1.0f);

  gfx::Transform translate;
  invert_transform_ = inverted;
  invert_transform_.Scale(root_window_scale_, root_window_scale_);

  if (host_insets.top() != 0 || host_insets.left() != 0) {
    float device_scale_factor = ui::GetDeviceScaleFactor(root_window_->layer());
    float x_offset = host_insets.left() / device_scale_factor;
    float y_offset = host_insets.top() / device_scale_factor;
    translate.Translate(x_offset, y_offset);
    invert_transform_.Translate(-x_offset, -y_offset);
  }
  float inverted_scale = 1.0f / root_window_scale_;
  translate.Scale(inverted_scale, inverted_scale);

  transform_ = translate * transform;
}

gfx::Transform AshRootWindowTransformer::GetTransform() const {
  return transform_;
}

gfx::Transform AshRootWindowTransformer::GetInverseTransform() const {
  return invert_transform_;
}

gfx::Rect AshRootWindowTransformer::GetRootWindowBounds(
    const gfx::Size& host_size) const {
  gfx::Rect bounds(host_size);
  bounds.Inset(host_insets_);
  bounds = ui::ConvertRectToDIP(root_window_->layer(), bounds);
  gfx::RectF new_bounds(bounds);
  root_window_->layer()->transform().TransformRect(&new_bounds);
  // It makes little sense to scale beyond the original
  // resolution.
  DCHECK_LE(root_window_scale_,
            ui::GetDeviceScaleFactor(root_window_->layer()));
  // Apply |root_window_scale_| twice as the downscaling
  // is already applied once in |SetTransformInternal()|.
  // TODO(oshima): This is a bit ugly. Consider specifying
  // the pseudo host resolution instead.
  new_bounds.Scale(root_window_scale_ * root_window_scale_);
  // Ignore the origin because RootWindow's insets are handled by
  // the transform.
  // Floor the size because the bounds is no longer aligned to
  // backing pixel when |root_window_scale_| is specified
  // (850 height at 1.25 scale becomes 1062.5 for example.)
  return gfx::Rect(gfx::ToFlooredSize(new_bounds.size()));
}

gfx::Insets AshRootWindowTransformer::GetHostInsets() const {
  return host_insets_;
}

}  // namespace ash
