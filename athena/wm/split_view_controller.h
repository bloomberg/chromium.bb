// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_
#define ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_

#include "athena/athena_export.h"
#include "athena/util/drag_handle.h"
#include "athena/wm/public/window_list_provider_observer.h"
#include "athena/wm/public/window_manager_observer.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"

namespace gfx {
class Rect;
class Transform;
}

namespace aura {
class ScopedWindowTargeter;
class Window;
class WindowTargeter;
}

namespace views {
class ViewTargeterDelegate;
class Widget;
}

namespace athena {
class SplitViewControllerTest;
class WindowListProviderImpl;

// Responsible for entering split view mode, exiting from split view mode, and
// laying out the windows in split view mode.
class ATHENA_EXPORT SplitViewController
    : public DragHandleScrollDelegate,
      public WindowManagerObserver,
      public WindowListProviderObserver {
 public:
  SplitViewController(aura::Window* container,
                      WindowListProviderImpl* window_list_provider);

  ~SplitViewController() override;

  bool CanActivateSplitViewMode() const;
  bool IsSplitViewModeActive() const;

  // Activates split-view mode with |left| and |right| windows. If |left| and/or
  // |right| is nullptr, then the first window in the window-list (which is
  // neither
  // |left| nor |right|) is selected instead. |to_activate| indicates which of
  // |left| or |right| should be activated. If |to_activate| is nullptr, the
  // currently active window is kept active if it is one of the split-view
  // windows.
  void ActivateSplitMode(aura::Window* left,
                         aura::Window* right,
                         aura::Window* to_activate);

  // Resets the internal state to an inactive state.
  void DeactivateSplitMode();

  // Replaces |window| in split-view mode with |replace_with|. Adjusts
  // |replace_with|'s visibility, transform and bounds. Resets |window|'s
  // visibility and transform but does not change its bounds.
  void ReplaceWindow(aura::Window* window,
                     aura::Window* replace_with);

  // Returns the bounds of the left and right parts of the |container_| based
  // on the current value of |divider_position_|.
  gfx::Rect GetLeftAreaBounds();
  gfx::Rect GetRightAreaBounds();

  aura::Window* left_window() { return left_window_; }
  aura::Window* right_window() { return right_window_; }

 private:
  friend class SplitViewControllerTest;

  enum State {
    // Split View mode is not active. |left_window_| and |right_window| are
    // nullptr.
    INACTIVE,
    // Two windows |left_window_| and |right_window| are shown side by side and
    // there is a horizontal scroll in progress which is dragging the divider
    // between the two windows.
    SCROLLING,
    // Split View mode is active with |left_window_| and |right_window| showing
    // side by side each occupying half the screen. No scroll in progress.
    ACTIVE
  };

  void SetState(State state);

  void InitializeDivider();
  void HideDivider();
  void ShowDivider();

  void UpdateLayout(bool animate);

  void SetWindowTransforms(const gfx::Transform& left_transform,
                           const gfx::Transform& right_transform,
                           const gfx::Transform& divider_transform,
                           bool animate);

  // Called when the animation initiated by SetWindowTransforms() completes.
  void OnAnimationCompleted();

  // Returns the default divider position for when the split view mode is
  // active and the divider is not being dragged.
  int GetDefaultDividerPosition();

  // Access to constants in anonymous namespace for testing purposes.
  float GetMaxDistanceFromMiddleForTest() const;
  float GetMinFlingVelocityForTest() const;

  // DragHandleScrollDelegate:
  void HandleScrollBegin(float delta) override;
  void HandleScrollEnd(float velocity) override;
  void HandleScrollUpdate(float delta) override;

  // WindowManagerObserver:
  void OnOverviewModeEnter() override;
  void OnOverviewModeExit() override;
  void OnSplitViewModeEnter() override;
  void OnSplitViewModeExit() override;

  // WindowListProviderObserver:
  void OnWindowStackingChangedInList() override {}
  void OnWindowAddedToList(aura::Window* added_window) override;
  void OnWindowRemovedFromList(aura::Window* removed_window,
                               int index) override;

  State state_;

  aura::Window* container_;

  // Provider of the list of windows to cycle through. Not owned.
  WindowListProviderImpl* window_list_provider_;

  // Windows for the left and right activities shown in SCROLLING and ACTIVE
  // states. In INACTIVE state these are nullptr.
  aura::Window* left_window_;
  aura::Window* right_window_;

  // X-Coordinate of the (center of the) divider between left_window_ and
  // right_window_ in |container_| coordinates.
  int divider_position_;

  // Meaningful only when state_ is SCROLLING.
  // X-Coordinate of the divider when the scroll started.
  int divider_scroll_start_position_;

  // Visually separates the windows and contains the drag handle.
  views::Widget* divider_widget_;

  // The drag handle which can be used when split view is active to exit the
  // split view mode.
  views::View* drag_handle_;

  scoped_ptr<aura::ScopedWindowTargeter> window_targeter_;
  scoped_ptr<views::ViewTargeterDelegate> view_targeter_delegate_;

  // Windows which should be hidden when the animation initiated by
  // UpdateLayout() completes.
  std::vector<aura::Window*> to_hide_;

  base::WeakPtrFactory<SplitViewController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewController);
};

}  // namespace athena

#endif  // ATHENA_WM_SPLIT_VIEW_CONTROLLER_H_
