// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
#define ASH_COMMON_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/common/wm/overview/overview_animation_type.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

class SkRegion;

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

class ScopedOverviewAnimationSettings;
class WmWindow;

// Manages a window, and it's transient children, in the overview mode. This
// class allows transforming the windows with a helper to determine the best
// fit in certain bounds. The window's state is restored on destruction of this
// object.
class ASH_EXPORT ScopedTransformOverviewWindow : public ui::EventHandler {
 public:
  class OverviewContentMask;
  using ScopedAnimationSettings =
      std::vector<std::unique_ptr<ScopedOverviewAnimationSettings>>;

  // Calculates and returns an optimal scale ratio. With MD this is only
  // taking into account |size.height()| as the width can vary. Without MD this
  // returns the scale that allows the item to fully fit within |size|.
  static float GetItemScale(const gfx::Size& source,
                            const gfx::Size& target,
                            int top_view_inset,
                            int title_height);

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio). Takes into account a window header that is |top_view_inset|
  // tall in the original window getting replaced by a window caption that is
  // |title_height| tall in transformed window.
  static gfx::Rect ShrinkRectToFitPreservingAspectRatio(const gfx::Rect& rect,
                                                        const gfx::Rect& bounds,
                                                        int top_view_inset,
                                                        int title_height);

  // Returns the transform turning |src_rect| into |dst_rect|.
  static gfx::Transform GetTransformForRect(const gfx::Rect& src_rect,
                                            const gfx::Rect& dst_rect);

  explicit ScopedTransformOverviewWindow(WmWindow* window);
  ~ScopedTransformOverviewWindow() override;

  // Starts an animation sequence which will use animation settings specified by
  // |animation_type|. The |animation_settings| container is populated with
  // scoped entities and the container should be destroyed at the end of the
  // animation sequence.
  //
  // Example:
  //  ScopedTransformOverviewWindow overview_window(window);
  //  ScopedTransformOverviewWindow::ScopedAnimationSettings scoped_settings;
  //  overview_window.BeginScopedAnimation(
  //      OverviewAnimationType::OVERVIEW_ANIMATION_SELECTOR_ITEM_SCROLL_CANCEL,
  //      &animation_settings);
  //  // Calls to SetTransform & SetOpacity will use the same animation settings
  //  // until scoped_settings is destroyed.
  //  overview_window.SetTransform(root_window, new_transform);
  //  overview_window.SetOpacity(1);
  void BeginScopedAnimation(OverviewAnimationType animation_type,
                            ScopedAnimationSettings* animation_settings);

  // Returns true if this window selector window contains the |target|.
  bool Contains(const WmWindow* target) const;

  // Returns the original target bounds of all transformed windows.
  gfx::Rect GetTargetBoundsInScreen() const;

  // Calculates the bounds of a |window_| after being transformed to the
  // selector's space. Those bounds are a union of all regular (normal and
  // panel) windows in the |window_|'s transient hierarchy. The returned Rect is
  // in virtual screen coordinates. The returned bounds are adjusted to allow
  // the original |window_|'s header to be hidden.
  gfx::Rect GetTransformedBounds() const;

  // Returns TOP_VIEW_COLOR property of |window_| unless there are transient
  // ancestors in which case returns SK_ColorTRANSPARENT.
  SkColor GetTopColor() const;

  // Returns TOP_VIEW_INSET property of |window_| unless there are transient
  // ancestors in which case returns 0.
  int GetTopInset() const;

  // Restores and animates the managed window to it's non overview mode state.
  void RestoreWindow();

  // Informs the ScopedTransformOverviewWindow that the window being watched was
  // destroyed. This resets the internal window pointer.
  void OnWindowDestroyed();

  // Prepares for overview mode by doing any necessary actions before entering.
  void PrepareForOverview();

  // Applies the |transform| to the overview window and all of its transient
  // children.
  void SetTransform(WmWindow* root_window, const gfx::Transform& transform);

  // Set's the opacity of the managed windows.
  void SetOpacity(float opacity);

  // Hides the window header whose size is given in |TOP_VIEW_INSET|
  // window property.
  void HideHeader();

  // Shows the window header that is hidden by HideHeader().
  void ShowHeader();

  // Creates/Deletes a mirror window for minimized windows.
  void UpdateMirrorWindowForMinimizedState();

  WmWindow* window() const { return window_; }

  // Closes the transient root of the window managed by |this|.
  void Close();

  // Returns the window used to show the content in overview mdoe.
  // For minmiezd window, this will be a window that hosts mirrored layers.
  WmWindow* GetOverviewWindow() const;

  // Ensures that a window is visible by setting its opacity to 1.
  void EnsureVisible();

  // Returns the window created for minimized window, or nullptr
  // if it does not exit.
  WmWindow* GetOverviewWindowForMinimizedState() const;

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  friend class WindowSelectorTest;

  // Closes the window managed by |this|.
  void CloseWidget();

  void CreateMirrorWindowForMinimizedState();

  // Makes Close() execute synchronously when used in tests.
  static void SetImmediateCloseForTests();

  // A weak pointer to the real window in the overview.
  WmWindow* window_;

  // Original window shape, if it was set on a window.
  std::unique_ptr<SkRegion> original_window_shape_;

  // True after the |original_window_shape_| has been set or after it has
  // been determined that window shape was not originally set on the |window_|.
  bool determined_original_window_shape_;

  // Tracks if this window was ignored by the shelf.
  bool ignored_by_shelf_;

  // True if the window has been transformed for overview mode.
  bool overview_started_;

  // The original transform of the window before entering overview mode.
  gfx::Transform original_transform_;

  // The original opacity of the window before entering overview mode.
  float original_opacity_;

  // A window that holds the content for minimized window.
  std::unique_ptr<views::Widget> minimized_widget_;

  base::WeakPtrFactory<ScopedTransformOverviewWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformOverviewWindow);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
