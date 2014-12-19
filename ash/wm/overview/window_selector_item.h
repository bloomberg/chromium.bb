// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/transparent_activate_window_button.h"
#include "ash/wm/overview/transparent_activate_window_button_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"

namespace aura {
class Window;
}

namespace views {
class Label;
class Widget;
}

namespace ash {

// This class represents an item in overview mode. An item can have one or more
// windows, of which only one can be activated by keyboard (i.e. alt+tab) but
// any can be selected with a pointer (touch or mouse).
class ASH_EXPORT WindowSelectorItem
    : public views::ButtonListener,
      public aura::WindowObserver,
      public TransparentActivateWindowButtonDelegate {
 public:
  explicit WindowSelectorItem(aura::Window* root_window);
  ~WindowSelectorItem() override;

  // Returns the root window on which this item is shown.
  aura::Window* root_window() { return root_window_; }

  // Adds a window to this selector item. Windows should be added in reverse
  // visible order so that the transparent overlay ordering matches the visible
  // ordering. Each |window| should have the same root window as |this| was
  // instantiated with.
  void AddWindow(aura::Window* window);

  // Returns true if the window selector item has |window| as a selectable
  // window.
  bool HasSelectableWindow(const aura::Window* window) const;

  // Returns true if |target| is contained in this WindowSelectorItem.
  bool Contains(const aura::Window* target) const;

  // Restores |window| on exiting window overview rather than returning it
  // to its previous state.
  void RestoreWindowOnExit(aura::Window* window);

  // Returns the |window| to activate on selecting of this item.
  aura::Window* SelectionWindow() const;

  // Removes |window| from this item. Check empty() after calling this to see
  // if the entire item is now empty.
  void RemoveWindow(const aura::Window* window);

  // Returns true if this item has no more selectable windows (i.e. after
  // calling RemoveWindow for the last contained window).
  bool empty() const;

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  void PrepareForOverview();

  // Sets the bounds of this window selector item to |target_bounds| in the
  // |root_window| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(aura::Window* root_window,
                 const gfx::Rect& target_bounds,
                 OverviewAnimationType animation_type);

  // Recomputes the positions for the windows in this selection item. This is
  // dispatched when the bounds of a window change.
  void RecomputeWindowTransforms();

  // Sends an a11y focus alert so that, if chromevox is enabled, the window
  // label is read.
  void SendFocusAlert() const;

  // Sets if the item is dimmed in the overview. Changing the value will also
  // change the visibility of the transform windows.
  void SetDimmed(bool dimmed);
  bool dimmed() const { return dimmed_; }

  const gfx::Rect& target_bounds() const { return target_bounds_; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // aura::WindowObserver:
  void OnWindowTitleChanged(aura::Window* window) override;

  // ash::TransparentActivateWindowButtonDelegate:
  void Select() override;

 private:
  friend class WindowSelectorTest;

  typedef ScopedVector<ScopedTransformOverviewWindow> TransformWindows;

  // Sets the bounds of this selector's items to |target_bounds| in
  // |root_window|. The bounds change will be animated as specified
  // by |animation_type|.
  void SetItemBounds(aura::Window* root_window,
                     const gfx::Rect& target_bounds,
                     OverviewAnimationType animation_type);

  // Changes the opacity of all the windows the item owns.
  void SetOpacity(float opacity);

  // Updates the window label's bounds to |target_bounds|. Will create a new
  // window label and fade it in if it doesn't exist. The bounds change is
  // animated as specified by the |animation_type|.
  void UpdateWindowLabels(const gfx::Rect& target_bounds,
                          OverviewAnimationType animation_type);

  // Initializes window_label_.
  void CreateWindowLabel(const base::string16& title);

  // Updates the bounds and accessibility names for all the transparent
  // overlays.
  void UpdateSelectorButtons();

  // Updates the close button's bounds. Any change in bounds will be animated
  // from the current bounds to the new bounds as per the |animation_type|.
  void UpdateCloseButtonLayout(OverviewAnimationType animation_type);

  // Updates the close buttons accessibility name.
  void UpdateCloseButtonAccessibilityName();

  // Returns the ScopedTransformOverviewWindow to activate or close.
  ScopedTransformOverviewWindow* SelectionTransformWindow() const;

  // True if the item is being shown in the overview, false if it's being
  // filtered.
  bool dimmed_;

  // The root window this item is being displayed on.
  aura::Window* root_window_;

  // The target bounds this selector item is fit within.
  gfx::Rect target_bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_;

  // Label under the window displaying its active tab name.
  scoped_ptr<views::Widget> window_label_;

  // View for the label under the window.
  views::Label* window_label_view_;

  // The close buttons widget container.
  views::Widget close_button_widget_;

  // An easy to access close button for the window in this item. Owned by the
  // close_button_widget_.
  views::ImageButton* close_button_;

  // Transparent overlay that covers the entire bounds of the
  // WindowSelectorItem and is stacked in front of all windows but behind each
  // Windows' own TransparentActivateWindowButton.
  scoped_ptr<TransparentActivateWindowButton>
      selector_item_activate_window_button_;

  // List of all Windows added to this and their associated helper classes.
  TransformWindows transform_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorItem);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
