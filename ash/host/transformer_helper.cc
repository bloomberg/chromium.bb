// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/transformer_helper.h"

#include "ash/host/ash_window_tree_host.h"
#include "ash/host/root_window_transformer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/transform.h"

namespace ash {
namespace {

// A simple RootWindowTransformer without host insets.
class SimpleRootWindowTransformer : public RootWindowTransformer {
 public:
  SimpleRootWindowTransformer(const aura::Window* root_window,
                              const gfx::Transform& transform)
      : root_window_(root_window), transform_(transform) {}

  // RootWindowTransformer overrides:
  virtual gfx::Transform GetTransform() const OVERRIDE { return transform_; }

  virtual gfx::Transform GetInverseTransform() const OVERRIDE {
    gfx::Transform invert;
    if (!transform_.GetInverse(&invert))
      return transform_;
    return invert;
  }

  virtual gfx::Rect GetRootWindowBounds(const gfx::Size& host_size) const
      OVERRIDE {
    gfx::Rect bounds(host_size);
    gfx::RectF new_bounds(ui::ConvertRectToDIP(root_window_->layer(), bounds));
    transform_.TransformRect(&new_bounds);
    return gfx::Rect(gfx::ToFlooredSize(new_bounds.size()));
  }

  virtual gfx::Insets GetHostInsets() const OVERRIDE { return gfx::Insets(); }

 private:
  virtual ~SimpleRootWindowTransformer() {}

  const aura::Window* root_window_;
  const gfx::Transform transform_;

  DISALLOW_COPY_AND_ASSIGN(SimpleRootWindowTransformer);
};

}  // namespace

TransformerHelper::TransformerHelper(AshWindowTreeHost* ash_host)
    : ash_host_(ash_host) {
  SetTransform(gfx::Transform());
}

TransformerHelper::~TransformerHelper() {}

gfx::Insets TransformerHelper::GetHostInsets() const {
  return transformer_->GetHostInsets();
}

void TransformerHelper::SetTransform(const gfx::Transform& transform) {
  scoped_ptr<RootWindowTransformer> transformer(new SimpleRootWindowTransformer(
      ash_host_->AsWindowTreeHost()->window(), transform));
  SetRootWindowTransformer(transformer.Pass());
}

void TransformerHelper::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_ = transformer.Pass();
  aura::WindowTreeHost* host = ash_host_->AsWindowTreeHost();
  aura::Window* window = host->window();
  window->SetTransform(transformer_->GetTransform());
  // If the layer is not animating, then we need to update the root window
  // size immediately.
  if (!window->layer()->GetAnimator()->is_animating())
    host->UpdateRootWindowSize(host->GetBounds().size());
}

gfx::Transform TransformerHelper::GetTransform() const {
  float scale = ui::GetDeviceScaleFactor(
      ash_host_->AsWindowTreeHost()->window()->layer());
  gfx::Transform transform;
  transform.Scale(scale, scale);
  transform *= transformer_->GetTransform();
  return transform;
}

gfx::Transform TransformerHelper::GetInverseTransform() const {
  float scale = ui::GetDeviceScaleFactor(
      ash_host_->AsWindowTreeHost()->window()->layer());
  gfx::Transform transform;
  transform.Scale(1.0f / scale, 1.0f / scale);
  return transformer_->GetInverseTransform() * transform;
}

void TransformerHelper::UpdateWindowSize(const gfx::Size& host_size) {
  ash_host_->AsWindowTreeHost()->window()->SetBounds(
      transformer_->GetRootWindowBounds(host_size));
}

}  // namespace ash
