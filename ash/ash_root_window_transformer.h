// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_ROOT_WINDOW_TRANSFORMER_H_
#define ASH_ASH_ROOT_WINDOW_TRANSFORMER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "ui/aura/root_window_transformer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/transform.h"

namespace aura {
class RootWindow;
}

namespace ash {

// RootWindowTransformer for ash environment.
class ASH_EXPORT AshRootWindowTransformer : public aura::RootWindowTransformer {
 public:
  AshRootWindowTransformer(aura::RootWindow* root,
                           const gfx::Transform& transform,
                           const gfx::Transform& inverted,
                           const gfx::Insets& insets,
                           float root_window_scale);
  // aura::RootWindowTransformer overrides:
  virtual gfx::Transform GetTransform() const OVERRIDE;
  virtual gfx::Transform GetInverseTransform() const OVERRIDE;
  virtual gfx::Rect GetRootWindowBounds(
      const gfx::Size& host_size) const OVERRIDE;
  virtual gfx::Insets GetHostInsets() const OVERRIDE;

 private:
  virtual ~AshRootWindowTransformer() {}

  aura::RootWindow* root_window_;
  gfx::Transform transform_;

  // The accurate representation of the inverse of the |transform_|.
  // This is used to avoid computation error caused by
  // |gfx::Transform::GetInverse|.
  gfx::Transform invert_transform_;

  // The scale of the root window. This is used to expand the
  // area of the root window (useful in HighDPI display).
  // Note that this should not be confused with the device scale
  // factor, which specfies the pixel density of the display.
  float root_window_scale_;

  gfx::Insets host_insets_;

  DISALLOW_COPY_AND_ASSIGN(AshRootWindowTransformer);
};

}  // namespace ash

#endif  // ASH_ASH_ROOT_WINDOW_TRANSFORMER_H_
