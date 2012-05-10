// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_VIEW_H_
#define ASH_LAUNCHER_LAUNCHER_VIEW_H_
#pragma once

#include <utility>
#include <vector>

#include "ash/launcher/launcher_button_host.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/wm/shelf_auto_hide_behavior.h"
#include "base/observer_list.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

namespace views {
class BoundsAnimator;
class ImageButton;
class MenuRunner;
class ViewModel;
}

namespace ash {

namespace test {
class LauncherViewTestAPI;
}

class LauncherDelegate;
struct LauncherItem;
class LauncherIconObserver;
class LauncherModel;

namespace internal {

class LauncherButton;

class ASH_EXPORT LauncherView : public views::View,
                                public LauncherModelObserver,
                                public views::ButtonListener,
                                public LauncherButtonHost,
                                public views::ContextMenuController,
                                public views::FocusTraversable {
 public:
  LauncherView(LauncherModel* model, LauncherDelegate* delegate);
  virtual ~LauncherView();

  void Init();

  void SetAlignment(ShelfAlignment alignment);

  // Returns the ideal bounds of the specified item, or an empty rect if id
  // isn't know.
  gfx::Rect GetIdealBoundsOfItemIcon(LauncherID id);

  void AddIconObserver(LauncherIconObserver* observer);
  void RemoveIconObserver(LauncherIconObserver* observer);

  // Returns true if we're showing a menu.
  bool IsShowingMenu() const;

  views::View* GetAppListButtonView() const;

  // Overridden from FocusTraversable:
  virtual views::FocusSearch* GetFocusSearch() OVERRIDE;
  virtual FocusTraversable* GetFocusTraversableParent() OVERRIDE;
  virtual View* GetFocusTraversableParentView() OVERRIDE;

 private:
  friend class ash::test::LauncherViewTestAPI;

  class FadeOutAnimationDelegate;
  class StartFadeAnimationDelegate;

  struct IdealBounds {
    gfx::Rect overflow_bounds;
  };

  // Used in calculating ideal bounds.
  int primary_axis_coordinate(int x, int y) const {
    return is_horizontal_alignment() ? x : y;
  }

  bool is_horizontal_alignment() const {
    return alignment_ == SHELF_ALIGNMENT_BOTTOM;
  }

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  // Calculates the ideal bounds. The bounds of each button corresponding to an
  // item in the model is set in |view_model_|.
  void CalculateIdealBounds(IdealBounds* bounds);

  // Returns the index of the last view whose max primary axis coordinate is
  // less than |max_value|. Returns -1 if nothing fits, or there are no views.
  int DetermineLastVisibleIndex(int max_value);

  // Animates the bounds of each view to its ideal bounds.
  void AnimateToIdealBounds();

  // Creates the view used to represent |item|.
  views::View* CreateViewForItem(const LauncherItem& item);

  // Fades |view| from an opacity of 0 to 1. This is when adding a new item.
  void FadeIn(views::View* view);

  // Invoked when the mouse has moved enough to trigger a drag. Sets internal
  // state in preparation for the drag.
  void PrepareForDrag(const views::MouseEvent& event);

  // Invoked when the mouse is dragged. Updates the models as appropriate.
  void ContinueDrag(const views::MouseEvent& event);

  // Returns true if |typea| and |typeb| should be in the same drag range.
  bool SameDragType(LauncherItemType typea, LauncherItemType typeb) const;

  // Returns the range (in the model) the item at the specified index can be
  // dragged to.
  std::pair<int,int> GetDragRange(int index);

  // If there is a drag operation in progress it's canceled.
  void CancelDrag(views::View* deleted_view);

  // Common setup done for all children.
  void ConfigureChildView(views::View* view);

  // Returns the items whose icons are not shown because they don't fit.
  void GetOverflowItems(std::vector<LauncherItem>* items);

  // Shows the overflow menu.
  void ShowOverflowMenu();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual FocusTraversable* GetPaneFocusTraversable() OVERRIDE;

  // Overridden from LauncherModelObserver:
  virtual void LauncherItemAdded(int model_index) OVERRIDE;
  virtual void LauncherItemRemoved(int model_index, LauncherID id) OVERRIDE;
  virtual void LauncherItemChanged(int model_index,
                                   const ash::LauncherItem& old_item) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;

  // Overridden from LauncherButtonHost:
  virtual void MousePressedOnButton(views::View* view,
                                    const views::MouseEvent& event) OVERRIDE;
  virtual void MouseDraggedOnButton(views::View* view,
                                    const views::MouseEvent& event) OVERRIDE;
  virtual void MouseReleasedOnButton(views::View* view,
                                     bool canceled) OVERRIDE;
  virtual void MouseExitedButton(views::View* view) OVERRIDE;
  virtual ShelfAlignment GetShelfAlignment() const OVERRIDE;
  virtual string16 GetAccessibleName(const views::View* view) OVERRIDE;

  // Overriden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // Overriden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point) OVERRIDE;

  // The model; owned by Launcher.
  LauncherModel* model_;

  // Delegate; owned by Launcher.
  LauncherDelegate* delegate_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  scoped_ptr<views::ViewModel> view_model_;

  // Last index of a launcher button that is visible
  // (does not go into overflow).
  int last_visible_index_;

  scoped_ptr<views::BoundsAnimator> bounds_animator_;

  views::ImageButton* overflow_button_;

  // Are we dragging? This is only set if the mouse is dragged far enough to
  // trigger a drag.
  bool dragging_;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  views::View* drag_view_;

  // X coordinate of the mouse down event in |drag_view_|s coordinates.
  int drag_offset_;

  // Index |drag_view_| was initially at.
  int start_drag_index_;

  // Used for the context menu of a particular item.
  LauncherID context_menu_id_;

  scoped_ptr<views::FocusSearch> focus_search_;

#if !defined(OS_MACOSX)
  scoped_ptr<views::MenuRunner> overflow_menu_runner_;

  scoped_ptr<views::MenuRunner> launcher_menu_runner_;
#endif

  ObserverList<LauncherIconObserver> observers_;

  ShelfAlignment alignment_;

  DISALLOW_COPY_AND_ASSIGN(LauncherView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_VIEW_H_
