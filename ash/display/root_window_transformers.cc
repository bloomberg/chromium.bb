// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/root_window_transformers.h"

#include <cmath>

#include "ash/display/display_info.h"
#include "ash/display/display_manager.h"
#include "ash/host/root_window_transformer.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/utils/SkMatrix44.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_property.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform.h"

DECLARE_WINDOW_PROPERTY_TYPE(gfx::Display::Rotation);

namespace ash {
namespace {

#if defined(OS_WIN)
DEFINE_WINDOW_PROPERTY_KEY(gfx::Display::Rotation, kRotationPropertyKey,
                           gfx::Display::ROTATE_0);
#endif

// Round near zero value to zero.
void RoundNearZero(gfx::Transform* transform) {
  const float kEpsilon = 0.001f;
  SkMatrix44& matrix = transform->matrix();
  for (int x = 0; x < 4; ++x) {
    for (int y = 0; y < 4; ++y) {
      if (std::abs(SkMScalarToFloat(matrix.get(x, y))) < kEpsilon)
        matrix.set(x, y, SkFloatToMScalar(0.0f));
    }
  }
}

// TODO(oshima): Transformers should be able to adjust itself
// when the device scale factor is changed, instead of
// precalculating the transform using fixed value.

gfx::Transform CreateRotationTransform(aura::Window* root_window,
                                       const gfx::Display& display) {
  DisplayInfo info =
      Shell::GetInstance()->display_manager()->GetDisplayInfo(display.id());

  // TODO(oshima): Add animation. (crossfade+rotation, or just cross-fade)
#if defined(OS_WIN)
  // Windows 8 bots refused to resize the host window, and
  // updating the transform results in incorrectly resizing
  // the root window. Don't apply the transform unless
  // necessary so that unit tests pass on win8 bots.
  if (info.rotation() == root_window->GetProperty(kRotationPropertyKey))
    return gfx::Transform();
  root_window->SetProperty(kRotationPropertyKey, info.rotation());
#endif

  gfx::Transform rotate;
  // The origin is (0, 0), so the translate width/height must be reduced by
  // 1 pixel.
  float one_pixel = 1.0f / display.device_scale_factor();
  switch (info.rotation()) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      rotate.Translate(display.bounds().height() - one_pixel, 0);
      rotate.Rotate(90);
      break;
    case gfx::Display::ROTATE_270:
      rotate.Translate(0, display.bounds().width() - one_pixel);
      rotate.Rotate(270);
      break;
    case gfx::Display::ROTATE_180:
      rotate.Translate(display.bounds().width() - one_pixel,
                       display.bounds().height() - one_pixel);
      rotate.Rotate(180);
      break;
  }

  RoundNearZero(&rotate);
  return rotate;
}

gfx::Transform CreateMagnifierTransform(aura::Window* root_window) {
  MagnificationController* magnifier =
      Shell::GetInstance()->magnification_controller();
  float magnifier_scale = 1.f;
  gfx::Point magnifier_offset;
  if (magnifier && magnifier->IsEnabled()) {
    magnifier_scale = magnifier->GetScale();
    magnifier_offset = magnifier->GetWindowPosition();
  }
  gfx::Transform transform;
  if (magnifier_scale != 1.f) {
    transform.Scale(magnifier_scale, magnifier_scale);
    transform.Translate(-magnifier_offset.x(), -magnifier_offset.y());
  }
  return transform;
}

gfx::Transform CreateInsetsAndScaleTransform(const gfx::Insets& insets,
                                             float device_scale_factor,
                                             float ui_scale) {
  gfx::Transform transform;
  if (insets.top() != 0 || insets.left() != 0) {
    float x_offset = insets.left() / device_scale_factor;
    float y_offset = insets.top() / device_scale_factor;
    transform.Translate(x_offset, y_offset);
  }
  float inverted_scale = 1.0f / ui_scale;
  transform.Scale(inverted_scale, inverted_scale);
  return transform;
}

// RootWindowTransformer for ash environment.
class AshRootWindowTransformer : public RootWindowTransformer {
 public:
  AshRootWindowTransformer(aura::Window* root,
                           const gfx::Display& display)
      : root_window_(root) {
    DisplayInfo info = Shell::GetInstance()->display_manager()->
        GetDisplayInfo(display.id());
    host_insets_ = info.GetOverscanInsetsInPixel();
    root_window_ui_scale_ = info.GetEffectiveUIScale();
    root_window_bounds_transform_ =
        CreateInsetsAndScaleTransform(host_insets_,
                                      display.device_scale_factor(),
                                      root_window_ui_scale_) *
        CreateRotationTransform(root, display);
    transform_ = root_window_bounds_transform_ * CreateMagnifierTransform(root);
    CHECK(transform_.GetInverse(&invert_transform_));
  }

  // aura::RootWindowTransformer overrides:
  virtual gfx::Transform GetTransform() const OVERRIDE {
    return transform_;
  }
  virtual gfx::Transform GetInverseTransform() const OVERRIDE {
    return invert_transform_;
  }
  virtual gfx::Rect GetRootWindowBounds(
      const gfx::Size& host_size) const OVERRIDE {
    gfx::Rect bounds(host_size);
    bounds.Inset(host_insets_);
    bounds = ui::ConvertRectToDIP(root_window_->layer(), bounds);
    gfx::RectF new_bounds(bounds);
    root_window_bounds_transform_.TransformRect(&new_bounds);
    // Apply |root_window_scale_| twice as the downscaling
    // is already applied once in |SetTransformInternal()|.
    // TODO(oshima): This is a bit ugly. Consider specifying
    // the pseudo host resolution instead.
    new_bounds.Scale(root_window_ui_scale_ * root_window_ui_scale_);
    // Ignore the origin because RootWindow's insets are handled by
    // the transform.
    // Floor the size because the bounds is no longer aligned to
    // backing pixel when |root_window_scale_| is specified
    // (850 height at 1.25 scale becomes 1062.5 for example.)
    return gfx::Rect(gfx::ToFlooredSize(new_bounds.size()));
  }

  virtual gfx::Insets GetHostInsets() const OVERRIDE {
    return host_insets_;
  }

 private:
  virtual ~AshRootWindowTransformer() {}

  aura::Window* root_window_;
  gfx::Transform transform_;

  // The accurate representation of the inverse of the |transform_|.
  // This is used to avoid computation error caused by
  // |gfx::Transform::GetInverse|.
  gfx::Transform invert_transform_;

  // The transform of the root window bounds. This is used to calculate
  // the size of root window.
  gfx::Transform root_window_bounds_transform_;

  // The scale of the root window. See |display_info::ui_scale_|
  // for more info.
  float root_window_ui_scale_;

  gfx::Insets host_insets_;

  DISALLOW_COPY_AND_ASSIGN(AshRootWindowTransformer);
};

// RootWindowTransformer for mirror root window. We simply copy the
// texture (bitmap) of the source display into the mirror window, so
// the root window bounds is the same as the source display's
// pixel size (excluding overscan insets).
class MirrorRootWindowTransformer : public RootWindowTransformer {
 public:
  MirrorRootWindowTransformer(const DisplayInfo& source_display_info,
                              const DisplayInfo& mirror_display_info) {
    root_bounds_ = gfx::Rect(source_display_info.bounds_in_native().size());
    gfx::Rect mirror_display_rect =
        gfx::Rect(mirror_display_info.bounds_in_native().size());

    bool letterbox = root_bounds_.width() * mirror_display_rect.height() >
        root_bounds_.height() * mirror_display_rect.width();
    if (letterbox) {
      float mirror_scale_ratio =
          (static_cast<float>(root_bounds_.width()) /
           static_cast<float>(mirror_display_rect.width()));
      float inverted_scale = 1.0f / mirror_scale_ratio;
      int margin = static_cast<int>(
          (mirror_display_rect.height() -
           root_bounds_.height() * inverted_scale) / 2);
      insets_.Set(0, margin, 0, margin);

      transform_.Translate(0,  margin);
      transform_.Scale(inverted_scale, inverted_scale);
    } else {
      float mirror_scale_ratio =
          (static_cast<float>(root_bounds_.height()) /
           static_cast<float>(mirror_display_rect.height()));
      float inverted_scale = 1.0f / mirror_scale_ratio;
      int margin = static_cast<int>(
          (mirror_display_rect.width() -
           root_bounds_.width() * inverted_scale) / 2);
      insets_.Set(margin, 0, margin, 0);

      transform_.Translate(margin, 0);
      transform_.Scale(inverted_scale, inverted_scale);
    }
  }

  // aura::RootWindowTransformer overrides:
  virtual gfx::Transform GetTransform() const OVERRIDE {
    return transform_;
  }
  virtual gfx::Transform GetInverseTransform() const OVERRIDE {
    gfx::Transform invert;
    CHECK(transform_.GetInverse(&invert));
    return invert;
  }
  virtual gfx::Rect GetRootWindowBounds(
      const gfx::Size& host_size) const OVERRIDE {
    return root_bounds_;
  }
  virtual gfx::Insets GetHostInsets() const OVERRIDE {
    return insets_;
  }

 private:
  virtual ~MirrorRootWindowTransformer() {}

  gfx::Transform transform_;
  gfx::Rect root_bounds_;
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(MirrorRootWindowTransformer);
};

}  // namespace

RootWindowTransformer* CreateRootWindowTransformerForDisplay(
    aura::Window* root,
    const gfx::Display& display) {
  return new AshRootWindowTransformer(root, display);
}

RootWindowTransformer* CreateRootWindowTransformerForMirroredDisplay(
    const DisplayInfo& source_display_info,
    const DisplayInfo& mirror_display_info) {
  return new MirrorRootWindowTransformer(source_display_info,
                                         mirror_display_info);
}

}  // namespace ash
