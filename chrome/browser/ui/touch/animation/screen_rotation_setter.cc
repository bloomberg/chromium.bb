// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/touch/animation/screen_rotation_setter.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/touch/animation/screen_rotation.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/interpolated_transform.h"
#include "views/layer_property_setter.h"
#include "views/view.h"

namespace {

static int SymmetricRound(float x) {
  return static_cast<int>(
    x > 0
      ? std::floor(x + 0.5f)
      : std::ceil(x - 0.5f));
}

// A screen rotation setter is a LayerPropertySetter that initiates screen
// rotations in response to calls to |SetTransform|. Calls to |SetBounds| are
// applied to the layer immediately.
class ScreenRotationSetter : public views::LayerPropertySetter,
                             public ScreenRotationListener {
 public:
  explicit ScreenRotationSetter(views::View* view);

  // implementation of LayerPropertySetter
  virtual void Installed(ui::Layer* layer) OVERRIDE;
  virtual void Uninstalled(ui::Layer* layer) OVERRIDE;
  virtual void SetTransform(ui::Layer* layer,
                            const ui::Transform& transform) OVERRIDE;
  virtual void SetBounds(ui::Layer* layer, const gfx::Rect& bounds) OVERRIDE;

  // implementation of ScreenRotationListener
  virtual void OnScreenRotationCompleted(const ui::Transform& final_transform,
                                         const gfx::Rect& final_rect) OVERRIDE;

 private:
  // This is the currently animating rotation. We hang onto it so that if a
  // call to |SetTransform| is made during the rotation, we can update the
  // target orientation of this rotation.
  scoped_ptr<ScreenRotation> rotation_;

  // If a call to SetBounds happens during a rotation its effect is delayed
  // until the rotation completes. If this happens several times, only the last
  // call to SetBounds will have any effect.
  scoped_ptr<gfx::Rect> pending_bounds_;

  // If a call to SetTransform happens during a rotation its effect is delayed
  // until the rotation completes. If this happens several times, only the last
  // call to SetTransform will have any effect.
  scoped_ptr<ui::Transform> pending_transform_;

  // The screen rotation setter is associated with a view so that the view's
  // bounds may be set when the animation completes.
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(ScreenRotationSetter);
};


ScreenRotationSetter::ScreenRotationSetter(views::View* view) : view_(view) {
}

void ScreenRotationSetter::Installed(ui::Layer* layer) OVERRIDE {
}

void ScreenRotationSetter::Uninstalled(ui::Layer* layer) OVERRIDE {
  if (rotation_.get())
    rotation_->Stop();
}

void ScreenRotationSetter::SetTransform(ui::Layer* layer,
                                        const ui::Transform& transform) {
  if (rotation_.get()) {
    pending_transform_.reset(new ui::Transform(transform));
  } else {
    float new_degrees, old_degrees;
    if (ui::InterpolatedTransform::FactorTRS(transform,
                                             NULL, &new_degrees, NULL) &&
        ui::InterpolatedTransform::FactorTRS(layer->transform(),
                                             NULL, &old_degrees, NULL)) {
      rotation_.reset(new ScreenRotation(view_,
                                         this,
                                         SymmetricRound(old_degrees),
                                         SymmetricRound(new_degrees)));
    }
  }
}

void ScreenRotationSetter::SetBounds(ui::Layer* layer,
                                     const gfx::Rect& bounds) {
  // cache bounds changes during an animation.
  if (rotation_.get())
    pending_bounds_.reset(new gfx::Rect(bounds));
  else
    layer->SetBounds(bounds);
}

void ScreenRotationSetter::OnScreenRotationCompleted(
    const ui::Transform& final_transform,
    const gfx::Rect& final_bounds) {
  // destroy the animation.
  rotation_.reset();

  if (pending_bounds_.get() && view_->layer()) {
    // If there are any pending bounds changes, they will have already been
    // applied to the view, and are waiting to be applied to the layer.
    view_->layer()->SetBounds(*pending_bounds_);
    pending_bounds_.reset();
  } else if (!final_bounds.IsEmpty()) {
    // Otherwise we may have new bounds as the result of the completed screen
    // rotation. Apply these to the view.
    view_->SetBoundsRect(final_bounds);
  }

  if (pending_transform_.get() && view_->layer()) {
    // If there is a pending transformation, we need to initiate another
    // animation.
    SetTransform(view_->layer(), *pending_transform_);
    pending_transform_.reset();
  }
}

}  // namespace

views::LayerPropertySetter* ScreenRotationSetterFactory::Create(
    views::View* view) {
  return new ScreenRotationSetter(view);
}
