// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_WIDGET_H_
#define ASH_COMMON_SHELF_SHELF_WIDGET_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_background_animator.h"
#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/shelf/shelf_layout_manager_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace app_list {
class ApplicationDragAndDropHost;
}

namespace ash {
class AppListButton;
class FocusCycler;
class ShelfLayoutManager;
class ShelfView;
class StatusAreaWidget;
class WmShelf;
class WmWindow;

class ASH_EXPORT ShelfWidget : public views::Widget,
                               public views::WidgetObserver,
                               public ShelfBackgroundAnimatorObserver,
                               public ShelfLayoutManagerObserver {
 public:
  ShelfWidget(WmWindow* shelf_container, WmShelf* wm_shelf);
  ~ShelfWidget() override;

  void CreateStatusAreaWidget(WmWindow* status_container);

  void OnShelfAlignmentChanged();

  // DEPRECATED: Prefer WmShelf::GetAlignment().
  ShelfAlignment GetAlignment() const;

  // Sets the shelf's background type.
  void SetPaintsBackground(ShelfBackgroundType background_type,
                           BackgroundAnimatorChangeType change_type);
  ShelfBackgroundType GetBackgroundType() const;

  // Hide the shelf behind a black bar during e.g. a user transition when |hide|
  // is true. The |animation_time_ms| will be used as animation duration.
  void HideShelfBehindBlackBar(bool hide, int animation_time_ms);
  bool IsShelfHiddenBehindBlackBar() const;

  // Causes shelf items to be slightly dimmed (e.g. when a window is maximized).
  void SetDimsShelf(bool dimming);
  bool GetDimsShelf() const;

  ShelfLayoutManager* shelf_layout_manager() { return shelf_layout_manager_; }
  StatusAreaWidget* status_area_widget() const { return status_area_widget_; }

  ShelfView* CreateShelfView();
  void PostCreateShelf();

  // Set visibility of the shelf.
  void SetShelfVisibility(bool visible);
  bool IsShelfVisible() const;

  bool IsShowingAppList() const;
  bool IsShowingContextMenu() const;
  bool IsShowingOverflowBubble() const;

  // Sets the focus cycler.  Also adds the shelf to the cycle.
  void SetFocusCycler(FocusCycler* focus_cycler);
  FocusCycler* GetFocusCycler();

  // Called by the activation delegate, before the shelf is activated
  // when no other windows are visible.
  void WillActivateAsFallback() { activating_as_fallback_ = true; }

  // Clean up prior to deletion.
  void Shutdown();

  // Force the shelf to be presented in an undimmed state.
  void ForceUndimming(bool force);

  // See WmShelf::UpdateIconPositionForPanel().
  void UpdateIconPositionForPanel(WmWindow* panel);

  // See WmShelf::GetScreenBoundsOfItemIconForWindow().
  gfx::Rect GetScreenBoundsOfItemIconForWindow(WmWindow* window);

  // Returns the button that opens the app launcher.
  AppListButton* GetAppListButton() const;

  // Returns the ApplicationDragAndDropHost for this shelf.
  app_list::ApplicationDragAndDropHost* GetDragAndDropHostForAppList();

  // Overridden from views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // A function to test the current alpha used by the dimming bar. If there is
  // no dimmer active, the function will return -1.
  int GetDimmingAlphaForTest();

  // A function to test the bounds of the dimming bar. Returns gfx::Rect() if
  // the dimmer is inactive.
  gfx::Rect GetDimmerBoundsForTest();

  // Disable dimming animations for running tests.
  void DisableDimmingAnimationsForTest();

  // ShelfBackgroundAnimatorObserver overrides:
  void UpdateShelfItemBackground(int alpha) override;

  // ShelfLayoutManagerObserver overrides:
  void WillDeleteShelfLayoutManager() override;

 private:
  class DelegateView;
  friend class DelegateView;

  WmShelf* wm_shelf_;

  // Owned by the shelf container's window.
  ShelfLayoutManager* shelf_layout_manager_;

  // Owned by the native widget.
  StatusAreaWidget* status_area_widget_;

  // |delegate_view_| is the contents view of this widget and is cleaned up
  // during CloseChildWindows of the associated RootWindowController.
  DelegateView* delegate_view_;
  // View containing the shelf items. Owned by the views hierarchy.
  ShelfView* shelf_view_;
  ShelfBackgroundAnimator background_animator_;
  bool activating_as_fallback_;

  DISALLOW_COPY_AND_ASSIGN(ShelfWidget);
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_WIDGET_H_
