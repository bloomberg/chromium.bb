// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
#define ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/wm/overview/scoped_transform_overview_window.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class SlideAnimation;
}

namespace views {
class ImageButton;
}

namespace ash {

class WindowSelector;
class WmWindow;

// This class represents an item in overview mode.
class ASH_EXPORT WindowSelectorItem : public views::ButtonListener,
                                      public aura::WindowObserver {
 public:
  // An image button with a close window icon.
  class OverviewCloseButton : public views::ImageButton {
   public:
    explicit OverviewCloseButton(views::ButtonListener* listener);
    ~OverviewCloseButton() override;

    // Resets the listener so that the listener can go out of scope.
    void ResetListener() { listener_ = nullptr; }

   private:
    DISALLOW_COPY_AND_ASSIGN(OverviewCloseButton);
  };

  WindowSelectorItem(WmWindow* window, WindowSelector* window_selector);
  ~WindowSelectorItem() override;

  WmWindow* GetWindow();

  // Returns the root window on which this item is shown.
  WmWindow* root_window() { return root_window_; }

  // Returns true if |target| is contained in this WindowSelectorItem.
  bool Contains(const WmWindow* target) const;

  // Restores and animates the managed window to its non overview mode state.
  void RestoreWindow();

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
  gfx::Rect GetTargetBoundsInScreen() const;

  // Sets the bounds of this window selector item to |target_bounds| in the
  // |root_window_| root window. The bounds change will be animated as specified
  // by |animation_type|.
  void SetBounds(const gfx::Rect& target_bounds,
                 OverviewAnimationType animation_type);

  // Activates or deactivates selection depending on |selected|.
  // In selected state the item's caption is shown transparent and blends with
  // the selection widget.
  void SetSelected(bool selected);

  // Sends an accessibility event indicating that this window became selected
  // so that it's highlighted and announced if accessibility features are
  // enabled.
  void SendAccessibleSelectionEvent();

  // Closes |transform_window_|.
  void CloseWindow();

  // Hides the original window header.
  void HideHeader();

  // Called when the window is minimized or unminimized.
  void OnMinimizedStateChanged();

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
  class CaptionContainerView;
  class RoundedContainerView;
  friend class WindowSelectorTest;

  enum class HeaderFadeInMode {
    ENTER,
    UPDATE,
    EXIT,
  };

  // Sets the bounds of this selector's items to |target_bounds| in
  // |root_window_|. The bounds change will be animated as specified
  // by |animation_type|.
  void SetItemBounds(const gfx::Rect& target_bounds,
                     OverviewAnimationType animation_type);

  // Changes the opacity of all the windows the item owns.
  void SetOpacity(float opacity);

  // Creates the window label.
  void CreateWindowLabel(const base::string16& title);

  // Updates the close button's and title label's bounds. Any change in bounds
  // will be animated from the current bounds to the new bounds as per the
  // |animation_type|. |mode| allows distinguishing the first time update which
  // allows setting the initial bounds properly or exiting overview to fade out
  // gradually.
  void UpdateHeaderLayout(HeaderFadeInMode mode,
                          OverviewAnimationType animation_type);

  // Animates opacity of the |transform_window_| and its caption to |opacity|
  // using |animation_type|.
  void AnimateOpacity(float opacity, OverviewAnimationType animation_type);

  // Updates the accessibility name to match the window title.
  void UpdateAccessibilityName();

  // Fades out a window caption when exiting overview mode.
  void FadeOut(std::unique_ptr<views::Widget> widget);

  // Allows a test to directly set animation state.
  gfx::SlideAnimation* GetBackgroundViewAnimation();

  WmWindow* GetOverviewWindowForMinimizedStateForTest();

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

  // True when |this| item is visually selected. Item header is made transparent
  // when the item is selected.
  bool selected_;

  // A widget that covers the |transform_window_|. The widget has
  // |caption_container_view_| as its contents view. The widget is backed by a
  // NOT_DRAWN layer since most of its surface is transparent.
  std::unique_ptr<views::Widget> item_widget_;

  // Container view that owns a Button view covering the |transform_window_|.
  // That button serves as an event shield to receive all events such as clicks
  // targeting the |transform_window_| or the overview header above the window.
  // The shield button owns |background_view_| which owns |label_view_|
  // and |close_button_|.
  CaptionContainerView* caption_container_view_;

  // A View for the text label above the window owned by the |background_view_|.
  views::Label* label_view_;

  // A close button for the window in this item owned by the |background_view_|.
  OverviewCloseButton* close_button_;

  // Pointer to the WindowSelector that owns the WindowGrid containing |this|.
  // Guaranteed to be non-null for the lifetime of |this|.
  WindowSelector* window_selector_;

  // Pointer to a view that covers the original header and has rounded top
  // corners. This view can have its color and opacity animated. It has a layer
  // which is the only textured layer used by the |item_widget_|.
  RoundedContainerView* background_view_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorItem);
};

}  // namespace ash

#endif  // ASH_COMMON_WM_OVERVIEW_WINDOW_SELECTOR_ITEM_H_
