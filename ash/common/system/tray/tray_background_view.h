// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_background_animator_observer.h"
#include "ash/common/system/tray/actionable_view.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace ash {
class TrayEventFilter;
class TrayBackground;
class WmShelf;

// Base class for children of StatusAreaWidget: SystemTray, WebNotificationTray,
// LogoutButtonTray, OverviewButtonTray.
// This class handles setting and animating the background when the Launcher
// is shown/hidden. It also inherits from ActionableView so that the tray
// items can override PerformAction when clicked on.
class ASH_EXPORT TrayBackgroundView : public ActionableView,
                                      public ui::ImplicitAnimationObserver,
                                      public ShelfBackgroundAnimatorObserver {
 public:
  static const char kViewClassName[];

  // Base class for tray containers. Sets the border and layout. The container
  // auto-resizes the widget when necessary.
  class TrayContainer : public views::View {
   public:
    explicit TrayContainer(ShelfAlignment alignment);
    ~TrayContainer() override {}

    void SetAlignment(ShelfAlignment alignment);

    void SetMargin(int main_axis_margin, int cross_axis_margin);

   protected:
    // views::View:
    void ChildPreferredSizeChanged(views::View* child) override;
    void ChildVisibilityChanged(View* child) override;
    void ViewHierarchyChanged(
        const ViewHierarchyChangedDetails& details) override;

   private:
    void UpdateLayout();

    ShelfAlignment alignment_;
    int main_axis_margin_ = 0;
    int cross_axis_margin_ = 0;

    DISALLOW_COPY_AND_ASSIGN(TrayContainer);
  };

  explicit TrayBackgroundView(WmShelf* wm_shelf);
  ~TrayBackgroundView() override;

  // Called after the tray has been added to the widget containing it.
  virtual void Initialize();

  // Initializes animations for the bubble.
  static void InitializeBubbleAnimations(views::Widget* bubble_widget);

  // views::View:
  void SetVisible(bool visible) override;
  const char* GetClassName() const override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  void OnPaint(gfx::Canvas* canvas) override;

  // ActionableView:
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;
  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Called whenever the shelf alignment changes.
  virtual void SetShelfAlignment(ShelfAlignment alignment);

  // Called when the anchor (tray or bubble) may have moved or changed.
  virtual void AnchorUpdated() {}

  // Called from GetAccessibleNodeData, must return a valid accessible name.
  virtual base::string16 GetAccessibleNameForTray() = 0;

  // Called when the bubble is resized.
  virtual void BubbleResized(const views::TrayBubbleView* bubble_view) {}

  // Hides the bubble associated with |bubble_view|. Called when the widget
  // is closed.
  virtual void HideBubbleWithView(const views::TrayBubbleView* bubble_view) = 0;

  // Called by the bubble wrapper when a click event occurs outside the bubble.
  // May close the bubble.
  virtual void ClickedOutsideBubble() = 0;

  // Sets |contents| as a child.
  void SetContents(views::View* contents);

  // Creates and sets contents background to |background_|. |draws_active|
  // determines if the view's background should be drawn as active when the view
  // is in the active state.
  void SetContentsBackground(bool draws_active);

  // Returns the bubble anchor alignment based on |shelf_alignment_|.
  views::TrayBubbleView::AnchorAlignment GetAnchorAlignment() const;

  void SetIsActive(bool is_active);
  bool is_active() const { return is_active_; }

  TrayContainer* tray_container() const { return tray_container_; }
  ShelfAlignment shelf_alignment() const { return shelf_alignment_; }
  TrayEventFilter* tray_event_filter() { return tray_event_filter_.get(); }
  WmShelf* shelf() { return wm_shelf_; }

  // Updates the arrow visibility based on the launcher visibility.
  void UpdateBubbleViewArrow(views::TrayBubbleView* bubble_view);

  // ShelfBackgroundAnimatorObserver:
  void UpdateShelfItemBackground(int alpha) override;

  // Updates the visibility of this tray's separator.
  void SetSeparatorVisibility(bool is_show);

  // Gets the anchor for bubbles, which is tray_container().
  views::View* GetBubbleAnchor() const;

  // Gets additional insets for positioning bubbles relative to
  // tray_container().
  gfx::Insets GetBubbleAnchorInsets() const;

 protected:
  // ActionableView:
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  bool ShouldEnterPushedState(const ui::Event& event) override;
  bool PerformAction(const ui::Event& event) override;
  void HandlePerformActionResult(bool action_performed,
                                 const ui::Event& event) override;
  gfx::Rect GetFocusBounds() override;
  void OnPaintFocus(gfx::Canvas* canvas) override;

 private:
  class TrayWidgetObserver;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;
  bool RequiresNotificationWhenAnimatorDestroyed() const override;

  // Applies transformations to the |layer()| to animate the view when
  // SetVisible(false) is called.
  void HideTransformation();

  // Helper function that calculates background insets relative to local bounds.
  gfx::Insets GetBackgroundInsets() const;

  // Helper function that calculates background bounds relative to local bounds
  // based on background insets returned from GetBackgroundInsets().
  gfx::Rect GetBackgroundBounds() const;

  // The shelf containing the system tray for this view.
  WmShelf* wm_shelf_;

  // Convenience pointer to the contents view.
  TrayContainer* tray_container_;

  // Shelf alignment.
  // TODO(jamescook): Don't cache this, get it from WmShelf.
  ShelfAlignment shelf_alignment_;

  // Owned by the view passed to SetContents().
  TrayBackground* background_;

  // Determines if the view is active. This changes how the background is drawn
  // in non-MD version and how the ink drop ripples behave in MD version.
  bool is_active_;

  // Visibility of this tray's separator which is a line of 1x32px and 4px to
  // right of tray.
  bool is_separator_visible_;

  std::unique_ptr<TrayWidgetObserver> widget_observer_;
  std::unique_ptr<TrayEventFilter> tray_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundView);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
