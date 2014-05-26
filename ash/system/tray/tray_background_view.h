// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_

#include "ash/ash_export.h"
#include "ash/shelf/background_animator.h"
#include "ash/shelf/shelf_types.h"
#include "ash/system/tray/actionable_view.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/bubble/tray_bubble_view.h"

namespace ash {
class ShelfLayoutManager;
class StatusAreaWidget;
class TrayEventFilter;
class TrayBackground;

// Base class for children of StatusAreaWidget: SystemTray, WebNotificationTray,
// LogoutButtonTray, OverviewButtonTray.
// This class handles setting and animating the background when the Launcher
// his shown/hidden. It also inherits from ActionableView so that the tray
// items can override PerformAction when clicked on.
class ASH_EXPORT TrayBackgroundView : public ActionableView,
                                      public BackgroundAnimatorDelegate,
                                      public ui::ImplicitAnimationObserver {
 public:
  static const char kViewClassName[];

  // Base class for tray containers. Sets the border and layout. The container
  // auto-resizes the widget when necessary.
  class TrayContainer : public views::View {
   public:
    explicit TrayContainer(ShelfAlignment alignment);
    virtual ~TrayContainer() {}

    void SetAlignment(ShelfAlignment alignment);

    void set_size(const gfx::Size& size) { size_ = size; }

    // views::View:
    virtual gfx::Size GetPreferredSize() const OVERRIDE;

   protected:
    // views::View:
    virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;
    virtual void ChildVisibilityChanged(View* child) OVERRIDE;
    virtual void ViewHierarchyChanged(
        const ViewHierarchyChangedDetails& details) OVERRIDE;

   private:
    void UpdateLayout();

    ShelfAlignment alignment_;
    gfx::Size size_;

    DISALLOW_COPY_AND_ASSIGN(TrayContainer);
  };

  explicit TrayBackgroundView(StatusAreaWidget* status_area_widget);
  virtual ~TrayBackgroundView();

  // Called after the tray has been added to the widget containing it.
  virtual void Initialize();

  // views::View:
  virtual void SetVisible(bool visible) OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;

  // ActionableView:
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;
  virtual gfx::Rect GetFocusBounds() OVERRIDE;

  // BackgroundAnimatorDelegate:
  virtual void UpdateBackground(int alpha) OVERRIDE;

  // Called whenever the shelf alignment changes.
  virtual void SetShelfAlignment(ShelfAlignment alignment);

  // Called when the anchor (tray or bubble) may have moved or changed.
  virtual void AnchorUpdated() {}

  // Called from GetAccessibleState, must return a valid accessible name.
  virtual base::string16 GetAccessibleNameForTray() = 0;

  // Called when the bubble is resized.
  virtual void BubbleResized(const views::TrayBubbleView* bubble_view) {}

  // Hides the bubble associated with |bubble_view|. Called when the widget
  // is closed.
  virtual void HideBubbleWithView(const views::TrayBubbleView* bubble_view) = 0;

  // Called by the bubble wrapper when a click event occurs outside the bubble.
  // May close the bubble. Returns true if the event is handled.
  virtual bool ClickedOutsideBubble() = 0;

  // Sets |contents| as a child.
  void SetContents(views::View* contents);

  // Creates and sets contents background to |background_|.
  void SetContentsBackground();

  // Sets whether the tray paints a background. Default is true, but is set to
  // false if a window overlaps the shelf.
  void SetPaintsBackground(bool value,
                           BackgroundAnimatorChangeType change_type);

  // Initializes animations for the bubble.
  void InitializeBubbleAnimations(views::Widget* bubble_widget);

  // Returns the window hosting the bubble.
  aura::Window* GetBubbleWindowContainer() const;

  // Returns the anchor rect for the bubble.
  gfx::Rect GetBubbleAnchorRect(
      views::Widget* anchor_widget,
      views::TrayBubbleView::AnchorType anchor_type,
      views::TrayBubbleView::AnchorAlignment anchor_alignment) const;

  // Returns the bubble anchor alignment based on |shelf_alignment_|.
  views::TrayBubbleView::AnchorAlignment GetAnchorAlignment() const;

  // Forces the background to be drawn active if set to true.
  void SetDrawBackgroundAsActive(bool visible);

  // Returns true when the the background was overridden to be drawn as active.
  bool draw_background_as_active() const {return draw_background_as_active_; }

  StatusAreaWidget* status_area_widget() {
    return status_area_widget_;
  }
  const StatusAreaWidget* status_area_widget() const {
    return status_area_widget_;
  }
  TrayContainer* tray_container() const { return tray_container_; }
  ShelfAlignment shelf_alignment() const { return shelf_alignment_; }
  TrayEventFilter* tray_event_filter() { return tray_event_filter_.get(); }

  ShelfLayoutManager* GetShelfLayoutManager();

  // Updates the arrow visibility based on the launcher visibility.
  void UpdateBubbleViewArrow(views::TrayBubbleView* bubble_view);

 private:
  class TrayWidgetObserver;

  // Called from Initialize after all status area trays have been created.
  // Sets the border based on the position of the view.
  void SetTrayBorder();

  // ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;

  // Applies transformations to the |layer()| to animate the view when
  // SetVisible(false) is called.
  void HideTransformation();

  // Unowned pointer to parent widget.
  StatusAreaWidget* status_area_widget_;

  // Convenience pointer to the contents view.
  TrayContainer* tray_container_;

  // Shelf alignment.
  ShelfAlignment shelf_alignment_;

  // Owned by the view passed to SetContents().
  TrayBackground* background_;

  // Animators for the background. They are only used for the old shelf layout.
  BackgroundAnimator hide_background_animator_;
  BackgroundAnimator hover_background_animator_;

  // True if the background gets hovered.
  bool hovered_;

  // This variable stores the activation override which will tint the background
  // differently if set to true.
  bool draw_background_as_active_;

  scoped_ptr<TrayWidgetObserver> widget_observer_;
  scoped_ptr<TrayEventFilter> tray_event_filter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBackgroundView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BACKGROUND_VIEW_H_
