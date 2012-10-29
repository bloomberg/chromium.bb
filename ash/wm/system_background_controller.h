// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SYSTEM_BACKGROUND_CONTROLLER_H_
#define ASH_WM_SYSTEM_BACKGROUND_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/root_window_observer.h"

namespace aura {
class RootWindow;
}

namespace ui {
class Layer;
}

namespace ash {
namespace internal {

// SystemBackgroundController manages a ui::Layer that's stacked at the bottom
// of an aura::RootWindow's children.  It exists solely to obscure portions of
// the root layer that aren't covered by any other layers (e.g. before the
// desktop background image is loaded at startup, or when we scale down all of
// the other layers as part of a power-button or window-management animation).
// It should never be transformed or restacked.
class ASH_EXPORT SystemBackgroundController : public aura::RootWindowObserver {
 public:
  enum Content {
    CONTENT_INVALID,

    // Copy the contents of the host window.  Useful when starting on X, where
    // the host window can inherit content from the X root window -- with this,
    // the compositor can continue drawing whatever was previously onscreen even
    // while we're e.g. waiting for images to be loaded from disk.
    CONTENT_COPY_FROM_HOST,

    // Display a black screen.
    CONTENT_BLACK,

#if defined(OS_CHROMEOS)
    // Display a solid-color screen matching the background color used for the
    // Chrome OS boot splash screen.
    CONTENT_CHROME_OS_BOOT_COLOR,
#endif
  };

  SystemBackgroundController(aura::RootWindow* root_window,
                             Content initial_content);
  virtual ~SystemBackgroundController();

  void SetContent(Content new_content);

  // aura::RootWindowObserver overrides:
  virtual void OnRootWindowResized(const aura::RootWindow* root,
                                   const gfx::Size& old_size) OVERRIDE;

 private:
  class HostContentLayerDelegate;

  // Initializes |layer_| as a textured layer containing a copy of the host
  // window's content.
  void CreateTexturedLayerWithHostContent();

  // Initializes |layer_| as a solid-color layer containing |color|.
  void CreateColoredLayer(SkColor color);

  // Updates |layer|'s bounds to match those of |root_window_|'s layer.
  void UpdateLayerBounds(ui::Layer* layer);

  // Adds |layer| to |root_window_|'s layer and stacks it at the bottom of all
  // its children.
  void AddLayerToRootLayer(ui::Layer* layer);

  aura::RootWindow* root_window_;  // not owned

  // Type of content currently displayed in |layer_|.
  Content content_;

  // Paints |layer_| when |content_| is CONTENT_COPY_FROM_HOST.
  scoped_ptr<HostContentLayerDelegate> layer_delegate_;

  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(SystemBackgroundController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SYSTEM_BACKGROUND_CONTROLLER_H_
