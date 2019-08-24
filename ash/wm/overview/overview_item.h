// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/caption_container_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/controls/button/button.h"

namespace ui {
class Shadow;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {
class DragWindowController;
class OverviewGrid;
class RoundedLabelWidget;

// This class represents an item in overview mode.
class ASH_EXPORT OverviewItem : public CaptionContainerView::EventDelegate,
                                public aura::WindowObserver,
                                public ui::ImplicitAnimationObserver,
                                public views::ButtonListener {
 public:
  OverviewItem(aura::Window* window,
               OverviewSession* overview,
               OverviewGrid* overview_grid);
  ~OverviewItem() override;

  aura::Window* GetWindow();

  // Returns the root window on which this item is shown.
  aura::Window* root_window() { return root_window_; }

  // Returns true if |target| is contained in this OverviewItem.
  bool Contains(const aura::Window* target) const;

  // Restores and animates the managed window to its non overview mode state.
  // If |reset_transform| equals false, the window's transform will not be
  // reset to identity transform when exiting overview mode. It's needed when
  // dragging an Arc app window in overview mode to put it in split screen. In
  // this case the restore of its transform needs to be deferred until the Arc
  // app window is snapped successfully, otherwise the animation will look very
  // ugly (the Arc app window enlarges itself to maximized window bounds and
  // then shrinks to its snapped window bounds). Note if the window's transform
  // is not reset here, it must be reset by someone else at some point.
  void RestoreWindow(bool reset_transform);

  // Ensures that a possibly minimized window becomes visible after restore.
  void EnsureVisible();

  // Restores stacking of window captions above the windows, then fades out.
  void Shutdown();

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  void PrepareForOverview();

  // Calculates and returns an optimal scale ratio. With MD this is only
  // taking into account |size.height()| as the width can vary. Without MD this
  // returns the scale that allows the item to fully fit within |size|.
  float GetItemScale(const gfx::Size& size);

  // Returns the union of the original target bounds of all transformed windows
  // managed by |this| item, i.e. all regular (normal or panel transient
  // descendants of the window returned by GetWindow()).
  gfx::RectF GetTargetBoundsInScreen() const;

  // Returns the transformed bound of |transform_window_|.
  gfx::RectF GetTransformedBounds() const;

  // Sets the bounds of this overview item to |target_bounds| in the
  // |root_window_| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(const gfx::RectF& target_bounds,
                 OverviewAnimationType animation_type);

  // Sends an accessibility event indicating that this window became selected
  // so that it is highlighted and announced.
  void SendAccessibleSelectionEvent();

  // Slides the item up or down and then closes the associated window. Used by
  // overview swipe to close.
  void AnimateAndCloseWindow(bool up);

  // Closes |transform_window_|.
  void CloseWindow();

  // Called when the window is minimized or unminimized.
  void OnMinimizedStateChanged();

  // Shows the cannot snap warning if currently in splitview, and the associated
  // window cannot be snapped.
  void UpdateCannotSnapWarningVisibility();

  // Called when a OverviewItem on any grid is dragged. Hides the close button
  // when a drag is started, and reshows it when a drag is finished.
  // Additionally hides the title and window icon if |item| is this.
  void OnSelectorItemDragStarted(OverviewItem* item);
  void OnSelectorItemDragEnded(bool snap);

  ScopedOverviewTransformWindow::GridWindowFillMode GetWindowDimensionsType()
      const;

  // Recalculates the window dimensions type of |transform_window_|. Called when
  // |window_|'s bounds change.
  void UpdateWindowDimensionsType();

  // TODO(minch): Do not actually scale up the item to get the bounds.
  // http://crbug.com/876567.
  // Returns the bounds of the selected item, which will be scaled up a little
  // bit and header view will be hidden after being selected. Note, the item
  // will be restored back after scaled up.
  gfx::Rect GetBoundsOfSelectedItem();

  // Increases the bounds of the dragged item.
  void ScaleUpSelectedItem(OverviewAnimationType animation_type);

  // Shift the window item up and then animates it to its original spot. Used
  // to transition from the home launcher.
  void SlideWindowIn();

  // Translate and fade the window (or minimized widget) and |item_widget_|. It
  // should remain in the same spot relative to the grids origin, which is given
  // by |new_grid_y|. Returns the settings object of the layer the caller should
  // observe.
  std::unique_ptr<ui::ScopedLayerAnimationSettings> UpdateYPositionAndOpacity(
      int new_grid_y,
      float opacity,
      OverviewSession::UpdateAnimationSettingsCallback callback);

  // If the window item represents a minimized window, update its content view.
  void UpdateItemContentViewForMinimizedWindow();

  // Checks if this item is current being dragged.
  bool IsDragItem();

  // Called after a positioning transform animation ends. Checks to see if the
  // animation was triggered by a drag end event. If so, inserts the window back
  // to its original stacking order so that the order of windows is the same as
  // when entering overview.
  void OnDragAnimationCompleted();

  // Updates |phantoms_for_dragging_|. If |phantoms_for_dragging_| is null, then
  // a new object is created for it.
  void UpdatePhantomsForDragging(bool is_touch_dragging);

  void DestroyPhantomsForDragging();

  // Sets the bounds of the window shadow. If |bounds_in_screen| is nullopt,
  // the shadow is hidden.
  void SetShadowBounds(base::Optional<gfx::Rect> bounds_in_screen);

  // Updates the rounded corners and shadow on this overview window item.
  void UpdateRoundedCornersAndShadow();

  // Called when the starting animation is completed, or called immediately
  // if there was no starting animation.
  void OnStartingAnimationComplete();

  // Stops the current animation of |item_widget_|.
  void StopWidgetAnimation();

  // Changes the opacity of all the windows the item owns.
  void SetOpacity(float opacity);
  float GetOpacity();

  OverviewAnimationType GetExitOverviewAnimationType();
  OverviewAnimationType GetExitTransformAnimationType();

  // If kNewOverviewLayout is on, use this function for handling events.
  void HandleGestureEventForTabletModeLayout(ui::GestureEvent* event);

  // CaptionContainerView::EventDelegate:
  void HandleMouseEvent(const ui::MouseEvent& event) override;
  void HandleGestureEvent(ui::GestureEvent* event) override;
  bool ShouldIgnoreGestureEvents() override;
  void OnHighlightedViewActivated() override;
  void OnHighlightedViewClosed() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  const gfx::RectF& target_bounds() const { return target_bounds_; }

  views::Widget* item_widget() { return item_widget_.get(); }

  CaptionContainerView* caption_container_view() {
    return caption_container_view_;
  }

  OverviewGrid* overview_grid() { return overview_grid_; }

  void set_should_animate_when_entering(bool should_animate) {
    should_animate_when_entering_ = should_animate;
  }
  bool should_animate_when_entering() const {
    return should_animate_when_entering_;
  }

  void set_should_animate_when_exiting(bool should_animate) {
    should_animate_when_exiting_ = should_animate;
  }
  bool should_animate_when_exiting() const {
    return should_animate_when_exiting_;
  }

  void set_should_restack_on_animation_end(bool val) {
    should_restack_on_animation_end_ = val;
  }

  bool animating_to_close() const { return animating_to_close_; }
  void set_animating_to_close(bool val) { animating_to_close_ = val; }

  void set_disable_mask(bool disable) { disable_mask_ = disable; }

  views::ImageButton* GetCloseButtonForTesting();
  float GetCloseButtonVisibilityForTesting() const;
  float GetTitlebarOpacityForTesting() const;
  gfx::Rect GetShadowBoundsForTesting();
  RoundedLabelWidget* cannot_snap_widget_for_testing() {
    return cannot_snap_widget_.get();
  }
  void set_target_bounds_for_testing(const gfx::RectF& target_bounds) {
    target_bounds_ = target_bounds;
  }

 private:
  friend class OverviewSessionRoundedCornerTest;
  friend class OverviewSessionTest;
  class OverviewCloseButton;
  FRIEND_TEST_ALL_PREFIXES(SplitViewOverviewSessionTest,
                           OverviewUnsnappableIndicatorVisibility);

  // Sets the bounds of this overview item to |target_bounds| in |root_window_|.
  // The bounds change will be animated as specified by |animation_type|.
  void SetItemBounds(const gfx::RectF& target_bounds,
                     OverviewAnimationType animation_type);

  // Creates the window label.
  void CreateWindowLabel();

  // Updates the |item_widget|'s bounds. Any change in bounds will be animated
  // from the current bounds to the new bounds as per the |animation_type|.
  void UpdateHeaderLayout(OverviewAnimationType animation_type);

  // Animates opacity of the |transform_window_| and its caption to |opacity|
  // using |animation_type|.
  void AnimateOpacity(float opacity, OverviewAnimationType animation_type);

  // Allows a test to directly set animation state.
  gfx::SlideAnimation* GetBackgroundViewAnimation();

  // Called before dragging. Scales up the window a little bit to indicate its
  // selection and stacks the window at the top of the Z order in order to keep
  // it visible while dragging around.
  void StartDrag();

  // TODO(sammiequon): Current events go from CaptionContainerView to
  // OverviewItem to OverviewSession to OverviewWindowDragController. We may be
  // able to shorten this pipeline.
  void HandlePressEvent(const gfx::PointF& location_in_screen,
                        bool from_touch_gesture);
  void HandleReleaseEvent(const gfx::PointF& location_in_screen);
  void HandleDragEvent(const gfx::PointF& location_in_screen);
  void HandleLongPressEvent(const gfx::PointF& location_in_screen);
  void HandleFlingStartEvent(const gfx::PointF& location_in_screen,
                             float velocity_x,
                             float velocity_y);
  void HandleTapEvent();
  void HandleGestureEndEvent();

  // Returns the list of windows that we want to slide up or down when swiping
  // on the shelf in tablet mode.
  aura::Window::Windows GetWindowsForHomeGesture();

  // The root window this item is being displayed on.
  aura::Window* root_window_;

  // The contained Window's wrapper.
  ScopedOverviewTransformWindow transform_window_;

  // The target bounds this overview item is fit within.
  gfx::RectF target_bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_ = false;

  // A widget stacked under the |transform_window_|. The widget has
  // |caption_container_view_| as its contents view. The widget is backed by a
  // NOT_DRAWN layer since most of its surface is transparent.
  std::unique_ptr<views::Widget> item_widget_;

  // The view associated with |item_widget_|. Contains a title, close button and
  // maybe a backdrop. Forwards certain events to |this|.
  CaptionContainerView* caption_container_view_ = nullptr;

  OverviewCloseButton* close_button_ = nullptr;

  // A widget with text that may show up on top of |transform_window_| to notify
  // users this window cannot be snapped.
  std::unique_ptr<RoundedLabelWidget> cannot_snap_widget_;

  // Responsible for phantoms that look like the window on all displays during
  // dragging.
  std::unique_ptr<DragWindowController> phantoms_for_dragging_;

  // Pointer to the Overview that owns the OverviewGrid containing |this|.
  // Guaranteed to be non-null for the lifetime of |this|.
  OverviewSession* overview_session_;

  // Pointer to the OverviewGrid that contains |this|. Guaranteed to be non-null
  // for the lifetime of |this|.
  OverviewGrid* overview_grid_;

  // True if the contained window should animate during the entering animation.
  bool should_animate_when_entering_ = true;

  // True if the contained window should animate during the exiting animation.
  bool should_animate_when_exiting_ = true;

  // True if after an animation, we need to reorder the stacking order of the
  // widgets.
  bool should_restack_on_animation_end_ = false;

  // True if the windows are still alive so they can have a closing animation.
  // These windows should not be used in calculations for
  // OverviewGrid::PositionWindows.
  bool animating_to_close_ = false;

  // True if this overview item is currently being dragged around.
  bool is_being_dragged_ = false;

  // True to always disable mask regardless of the state.
  bool disable_mask_ = false;

  // Stores the last translations of the windows affected by SetBounds. Used for
  // ease of calculations when swiping away overview mode using home launcher
  // gesture.
  base::flat_map<aura::Window*, int> translation_y_map_;

  // The shadow around the overview window. Shadows the original window, not
  // |item_widget_|. Done here instead of on the original window because of the
  // rounded edges mask applied on entering overview window.
  std::unique_ptr<ui::Shadow> shadow_;

  DISALLOW_COPY_AND_ASSIGN(OverviewItem);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ITEM_H_
