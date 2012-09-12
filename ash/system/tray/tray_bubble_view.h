// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_

#include "ash/wm/shelf_types.h"
#include "ui/aura/event_filter.h"
#include "ui/views/bubble/bubble_delegate.h"

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
class Widget;
}

namespace ash {
namespace internal {

// Specialized bubble view for status area tray bubbles.
// Mostly this handles custom anchor location and arrow and border rendering.
class TrayBubbleView : public views::BubbleDelegateView {
 public:
  enum AnchorType {
    ANCHOR_TYPE_TRAY,
    ANCHOR_TYPE_BUBBLE
  };

  class Host : public aura::EventFilter {
   public:
    Host();
    virtual ~Host();

    // Set widget_ and tray_view_, set up animations, and show the bubble.
    // Must occur after bubble_view->CreateBubble() is called.
    void InitializeAndShowBubble(views::Widget* widget,
                                 TrayBubbleView* bubble_view,
                                 views::View* tray_view);

    virtual void BubbleViewDestroyed() = 0;
    virtual void OnMouseEnteredView() = 0;
    virtual void OnMouseExitedView() = 0;
    virtual void OnClickedOutsideView() = 0;
    virtual string16 GetAccessibleName() = 0;

    // Overridden from aura::EventFilter.
    virtual bool PreHandleKeyEvent(aura::Window* target,
                                   ui::KeyEvent* event) OVERRIDE;
    virtual bool PreHandleMouseEvent(aura::Window* target,
                                     ui::MouseEvent* event) OVERRIDE;
    virtual ui::TouchStatus PreHandleTouchEvent(
        aura::Window* target,
        ui::TouchEvent* event) OVERRIDE;
    virtual ui::EventResult PreHandleGestureEvent(
        aura::Window* target,
        ui::GestureEvent* event) OVERRIDE;

   private:
    void ProcessLocatedEvent(aura::Window* target,
                             const ui::LocatedEvent& event);

    views::Widget* widget_;
    views::View* tray_view_;

    DISALLOW_COPY_AND_ASSIGN(Host);
  };

  struct InitParams {
    static const int kArrowDefaultOffset;

    InitParams(AnchorType anchor_type,
               ShelfAlignment shelf_alignment);
    AnchorType anchor_type;
    ShelfAlignment shelf_alignment;
    int bubble_width;
    int max_height;
    bool can_activate;
    bool close_on_deactivate;
    int arrow_offset;
    SkColor arrow_color;
  };

  static TrayBubbleView* Create(views::View* anchor,
                                Host* host,
                                const InitParams& init_params);

  virtual ~TrayBubbleView();

  // Called whenever the bubble size or location may have changed.
  void UpdateBubble();

  // Sets the maximum bubble height and resizes the bubble.
  void SetMaxHeight(int height);

  // Called when the host is destroyed.
  void reset_host() { host_ = NULL; }

  void set_gesture_dragging(bool dragging) { is_gesture_dragging_ = dragging; }
  bool is_gesture_dragging() const { return is_gesture_dragging_; }

  // Overridden from views::WidgetDelegate.
  virtual bool CanActivate() const OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;

  // Overridden from views::BubbleDelegateView.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  TrayBubbleView(const InitParams& init_params,
                 views::BubbleBorder::ArrowLocation arrow_location,
                 views::View* anchor,
                 Host* host);

  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  // Overridden from views::View.
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

 private:
  InitParams params_;
  Host* host_;
  bool is_gesture_dragging_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleView);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_
