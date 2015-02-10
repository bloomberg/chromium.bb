// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"

namespace aura {
class Window;
}

namespace views {
class LabelButton;
class Widget;
}

namespace ash {

// This class represents an item in overview mode.
class ASH_EXPORT WindowSelectorItem : public views::ButtonListener,
                                      public aura::WindowObserver {
 public:
  class OverviewLabelButton : public views::LabelButton {
   public:
    OverviewLabelButton(views::ButtonListener* listener,
                        const base::string16& text);

    ~OverviewLabelButton() override;

    void set_top_padding(int top_padding) { top_padding_ = top_padding; }

   protected:
    // views::LabelButton:
    gfx::Rect GetChildAreaBounds() override;

   private:
    // Padding on top of the button.
    int top_padding_;

    DISALLOW_COPY_AND_ASSIGN(OverviewLabelButton);
  };

  explicit WindowSelectorItem(aura::Window* window);
  ~WindowSelectorItem() override;

  aura::Window* GetWindow();

  // Returns the root window on which this item is shown.
  aura::Window* root_window() { return root_window_; }

  // Returns true if |target| is contained in this WindowSelectorItem.
  bool Contains(const aura::Window* target) const;

  // Restores and animates the managed window to it's non overview mode state.
  void RestoreWindow();

  // Forces the managed window to be shown (ie not hidden or minimized) when
  // calling RestoreWindow().
  void ShowWindowOnExit();

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  void PrepareForOverview();

  // Sets the bounds of this window selector item to |target_bounds| in the
  // |root_window_| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(const gfx::Rect& target_bounds,
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
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowTitleChanged(aura::Window* window) override;

 private:
  friend class WindowSelectorTest;

  // Sets the bounds of this selector's items to |target_bounds| in
  // |root_window_|. The bounds change will be animated as specified
  // by |animation_type|.
  void SetItemBounds(const gfx::Rect& target_bounds,
                     OverviewAnimationType animation_type);

  // Changes the opacity of all the windows the item owns.
  void SetOpacity(float opacity);

  // Updates the window label bounds.
  void UpdateWindowLabel(const gfx::Rect& window_bounds,
                         OverviewAnimationType animation_type);

  // Creates the window label.
  void CreateWindowLabel(const base::string16& title);

  // Updates the close button's bounds. Any change in bounds will be animated
  // from the current bounds to the new bounds as per the |animation_type|.
  void UpdateCloseButtonLayout(OverviewAnimationType animation_type);

  // Updates the close buttons accessibility name.
  void UpdateCloseButtonAccessibilityName();

  // True if the item is being shown in the overview, false if it's being
  // filtered.
  bool dimmed_;

  // The root window this item is being displayed on.
  aura::Window* root_window_;

  // The contained Window's wrapper.
  ScopedTransformOverviewWindow transform_window_;

  // The target bounds this selector item is fit within.
  gfx::Rect target_bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_;

  // Label under the window displaying its active tab name.
  scoped_ptr<views::Widget> window_label_;

  // View for the label under the window.
  OverviewLabelButton* window_label_button_view_;

  // The close buttons widget container.
  views::Widget close_button_widget_;

  // An easy to access close button for the window in this item. Owned by the
  // close_button_widget_.
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorItem);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
