// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/view.h"

namespace ui {
class LayerTreeOwner;
}

namespace ash {

class WmWindow;

namespace wm {

// A view that mirrors the client area of a single window.
class WindowMirrorView : public views::View {
 public:
  explicit WindowMirrorView(WmWindow* window);
  ~WindowMirrorView() override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;

 private:
  void InitLayerOwner();

  // Gets the root of the layer tree that was lifted from |target_| (and is now
  // a child of |this->layer()|).
  ui::Layer* GetMirrorLayer();

  // Calculates the bounds of the client area of the Window in the widget
  // coordinate space.
  gfx::Rect GetClientAreaBounds() const;

  // The original window that is being represented by |this|.
  WmWindow* target_;

  // Retains ownership of the mirror layer tree. This is lazily initialized
  // the first time the view becomes visible.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  DISALLOW_COPY_AND_ASSIGN(WindowMirrorView);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_H_
