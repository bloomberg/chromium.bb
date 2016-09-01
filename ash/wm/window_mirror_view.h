// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_MIRROR_VIEW_H_
#define ASH_WM_WINDOW_MIRROR_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/view.h"
#include "ui/wm/core/window_util.h"

namespace ash {

class WmWindowAura;

namespace wm {

class ForwardingLayerDelegate;

// A view that mirrors the client area of a single window. Layers are lifted
// from the underlying window (which gets new ones in their place). New paint
// calls, if any, are forwarded to the underlying window.
class WindowMirrorView : public views::View, public ::wm::LayerDelegateFactory {
 public:
  explicit WindowMirrorView(WmWindowAura* window);
  ~WindowMirrorView() override;

  // views::View:
  gfx::Size GetPreferredSize() const override;
  void Layout() override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;

  // ::wm::LayerDelegateFactory:
  ui::LayerDelegate* CreateDelegate(ui::Layer* new_layer,
                                    ui::Layer* layer) override;

 private:
  void InitLayerOwner();

  // Gets the root of the layer tree that was lifted from |target_| (and is now
  // a child of |this->layer()|).
  ui::Layer* GetMirrorLayer();

  // Calculates the bounds of the client area of the Window in the widget
  // coordinate space.
  gfx::Rect GetClientAreaBounds() const;

  // The original window that is being represented by |this|.
  WmWindowAura* target_;

  // Retains ownership of the mirror layer tree. This is lazily initialized
  // the first time the view becomes visible.
  std::unique_ptr<ui::LayerTreeOwner> layer_owner_;

  std::vector<std::unique_ptr<ForwardingLayerDelegate>> delegates_;

  DISALLOW_COPY_AND_ASSIGN(WindowMirrorView);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_MIRROR_VIEW_H_
