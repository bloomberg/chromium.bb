// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHADOW_H_
#define ASH_WM_SHADOW_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {
namespace internal {

class ImageGrid;

// Simple class that draws a drop shadow around content at given bounds.
class ASH_EXPORT Shadow : public ui::ImplicitAnimationObserver {
 public:
  enum Style {
    // Active windows have more opaque shadows, shifted down to make the window
    // appear "higher".
    STYLE_ACTIVE,

    // Inactive windows have less opaque shadows.
    STYLE_INACTIVE,

    // Small windows like tooltips and context menus have lighter, smaller
    // shadows.
    STYLE_SMALL,
  };

  Shadow();
  virtual ~Shadow();

  // |window| is a possibly-arbitrary window that is drawn at the same DPI
  // (i.e. on the same monitor) as this shadow. The actual bounds for the
  // shadow still has to be provided through |SetContentBounds()|.
  // TODO(oshima): move scale factor to ui/compositor/ and remove this.
  void Init(aura::Window* window, Style style);

  // Returns |image_grid_|'s ui::Layer.  This is exposed so it can be added to
  // the same layer as the content and stacked below it.  SetContentBounds()
  // should be used to adjust the shadow's size and position (rather than
  // applying transformations to this layer).
  ui::Layer* layer() const;

  const gfx::Rect& content_bounds() const { return content_bounds_; }
  Style style() const { return style_; }

  // Moves and resizes |image_grid_| to frame |content_bounds|.
  void SetContentBounds(const gfx::Rect& content_bounds);

  // Sets the shadow's style, animating opacity as necessary.
  void SetStyle(Style style);

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

 private:
  // Updates the |image_grid_| images to the current |style_|.
  void UpdateImagesForStyle();

  // Updates the |image_grid_| bounds based on its image sizes and the
  // current |content_bounds_|.
  void UpdateImageGridBounds();

  // The current style, set when the transition animation starts.
  Style style_;

  scoped_ptr<ImageGrid> image_grid_;

  // Bounds of the content that the shadow encloses.
  gfx::Rect content_bounds_;

  DISALLOW_COPY_AND_ASSIGN(Shadow);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SHADOW_H_
