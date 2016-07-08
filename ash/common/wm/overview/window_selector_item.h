// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
#define ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/wm/overview/scoped_transform_overview_window.h"
#include "ash/common/wm_window_observer.h"
#include "base/macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/widget/widget.h"

namespace views {
class ImageButton;
}

namespace ash {

class WindowSelector;
class WmWindow;

// This class represents an item in overview mode.
class ASH_EXPORT WindowSelectorItem : public views::ButtonListener,
                                      public WmWindowObserver {
 public:
  class OverviewLabelButton : public views::LabelButton {
   public:
    OverviewLabelButton(views::ButtonListener* listener,
                        const base::string16& text);

    ~OverviewLabelButton() override;

    // Makes sure that text is readable with |background_color|.
    void SetBackgroundColor(SkColor background_color);

    void set_padding(const gfx::Insets& padding) { padding_ = padding; }

   protected:
    // views::LabelButton:
    gfx::Rect GetChildAreaBounds() override;

   private:
    // Padding on all sides to correctly place the text inside the view.
    gfx::Insets padding_;

    DISALLOW_COPY_AND_ASSIGN(OverviewLabelButton);
  };

  WindowSelectorItem(WmWindow* window, WindowSelector* window_selector);
  ~WindowSelectorItem() override;

  WmWindow* GetWindow();

  // Returns the root window on which this item is shown.
  WmWindow* root_window() { return root_window_; }

  // Returns true if |target| is contained in this WindowSelectorItem.
  bool Contains(const WmWindow* target) const;

  // Restores and animates the managed window to it's non overview mode state.
  void RestoreWindow();

  // Forces the managed window to be shown (ie not hidden or minimized) when
  // calling RestoreWindow().
  void ShowWindowOnExit();

  // Dispatched before beginning window overview. This will do any necessary
  // one time actions such as restoring minimized windows.
  void PrepareForOverview();

  // Calculates and returns an optimal scale ratio. With MD this is only
  // taking into account |size.height()| as the width can vary. Without MD this
  // returns the scale that allows the item to fully fit within |size|.
  float GetItemScale(const gfx::Size& size);

  // Sets the bounds of this window selector item to |target_bounds| in the
  // |root_window_| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(const gfx::Rect& target_bounds,
                 OverviewAnimationType animation_type);

  // Activates or deactivates selection depending on |selected|.
  // Currently does nothing unless Material Design is enabled. With Material
  // Design the item's caption is shown transparent in selected state and blends
  // with the selection widget.
  void SetSelected(bool selected);

  // Recomputes the positions for the windows in this selection item. This is
  // dispatched when the bounds of a window change.
  void RecomputeWindowTransforms();

  // Sends an accessibility event indicating that this window became selected
  // so that it's highlighted and announced if accessibility features are
  // enabled.
  void SendAccessibleSelectionEvent();

  // Closes |transform_window_|.
  void CloseWindow();

  // Sets if the item is dimmed in the overview. Changing the value will also
  // change the visibility of the transform windows.
  void SetDimmed(bool dimmed);
  bool dimmed() const { return dimmed_; }

  const gfx::Rect& target_bounds() const { return target_bounds_; }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // WmWindowObserver:
  void OnWindowDestroying(WmWindow* window) override;
  void OnWindowTitleChanged(WmWindow* window) override;

 private:
  class CaptionContainerView;
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

  // Updates the close button's and title label's bounds. Any change in bounds
  // will be animated from the current bounds to the new bounds as per the
  // |animation_type|.
  void UpdateHeaderLayout(OverviewAnimationType animation_type);

  // Animates opacity of the |transform_window_| and its caption to |opacity|
  // using |animation_type|.
  void AnimateOpacity(float opacity, OverviewAnimationType animation_type);

  // Updates the close buttons accessibility name.
  void UpdateCloseButtonAccessibilityName();

  // True if the item is being shown in the overview, false if it's being
  // filtered.
  bool dimmed_;

  // The root window this item is being displayed on.
  WmWindow* root_window_;

  // The contained Window's wrapper.
  ScopedTransformOverviewWindow transform_window_;

  // The target bounds this selector item is fit within.
  gfx::Rect target_bounds_;

  // True if running SetItemBounds. This prevents recursive calls resulting from
  // the bounds update when calling ::wm::RecreateWindowLayers to copy
  // a window layer for display on another monitor.
  bool in_bounds_update_;

  // Label displaying its name (active tab for tabbed windows).
  // With Material Design this Widget owns |caption_container_view_| and is
  // shown above the |transform_window_|.
  // Otherwise it is shown under the window.
  std::unique_ptr<views::Widget> window_label_;

  // Label background widget used to fade in opacity when moving selection.
  std::unique_ptr<views::Widget> window_label_selector_;

  // Container view that owns |window_label_button_view_| and |close_button_|.
  // Only used with Material Design.
  CaptionContainerView* caption_container_view_;

  // View for the label below the window or (with material design) above it.
  OverviewLabelButton* window_label_button_view_;

  // The close buttons widget container. Not used with Material Design.
  std::unique_ptr<views::Widget> close_button_widget_;

  // A close button for the window in this item. Owned by the
  // |caption_container_view_| with Material Design or by |close_button_widget_|
  // otherwise.
  views::ImageButton* close_button_;

  // Pointer to the WindowSelector that owns the WindowGrid containing |this|.
  // Guaranteed to be non-null for the lifetime of |this|.
  WindowSelector* window_selector_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorItem);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
