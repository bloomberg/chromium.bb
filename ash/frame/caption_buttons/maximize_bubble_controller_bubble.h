// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_CAPTION_BUTTONS_MAXIMIZE_BUBBLE_CONTROLLER_BUBBLE_H_
#define ASH_FRAME_CAPTION_BUTTONS_MAXIMIZE_BUBBLE_CONTROLLER_BUBBLE_H_

#include "ash/wm/workspace/snap_types.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/mouse_watcher.h"

namespace views {
class CustomButton;
}

namespace ash {

class BubbleContentsView;
class MaximizeBubbleBorder;
class MaximizeBubbleController;

// The class which creates and manages the bubble menu element.
// It creates a 'bubble border' and the content accordingly.
// Note: Since the phantom window will show animations on top of the maximize
// button this menu gets created as a separate window and the phantom window
// will be created underneath this window.
class MaximizeBubbleControllerBubble : public views::BubbleDelegateView,
                                       public views::MouseWatcherListener {
 public:
  static const SkColor kBubbleBackgroundColor;
  static const int kLayoutSpacing;  // The spacing between two buttons.

  MaximizeBubbleControllerBubble(MaximizeBubbleController* owner,
                                 int appearance_delay_ms,
                                 SnapType initial_snap_type);
  virtual ~MaximizeBubbleControllerBubble();

  // The window of the menu under which the phantom window will get created.
  aura::Window* GetBubbleWindow();

  // Overridden from views::BubbleDelegateView.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;

  // Overridden from views::WidgetDelegateView.
  virtual bool WidgetHasHitTestMask() const OVERRIDE;
  virtual void GetWidgetHitTestMask(gfx::Path* mask) const OVERRIDE;

  // Implementation of MouseWatcherListener.
  virtual void MouseMovedOutOfHost() OVERRIDE;

  // Implementation of MouseWatcherHost.
  virtual bool Contains(const gfx::Point& screen_point,
                        views::MouseWatcherHost::MouseEventType type);

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;

  // Called from the controller class to indicate that the menu should get
  // destroyed.
  virtual void ControllerRequestsCloseAndDelete();

  // Called from the owning class to change the menu content to the given
  // |snap_type| so that the user knows what is selected.
  void SetSnapType(SnapType snap_type);

  // Get the owning MaximizeBubbleController. This might return NULL in case
  // of an asynchronous shutdown.
  MaximizeBubbleController* controller() const { return owner_; }

  // Added for unit test: Retrieve the button for an action.
  // |state| can be either SNAP_LEFT, SNAP_RIGHT or SNAP_MINIMIZE.
  views::CustomButton* GetButtonForUnitTest(SnapType state);

 private:
  // True if the shut down has been initiated.
  bool shutting_down_;

  // Our owning class.
  MaximizeBubbleController* owner_;

  // The content accessor of the menu.
  BubbleContentsView* contents_view_;

  // The bubble border (weak reference).
  MaximizeBubbleBorder* bubble_border_;

  // The mouse watcher which takes care of out of window hover events.
  scoped_ptr<views::MouseWatcher> mouse_watcher_;

  // The fade delay - if 0 it will show / hide immediately.
  const int appearance_delay_ms_;

  DISALLOW_COPY_AND_ASSIGN(MaximizeBubbleControllerBubble);
};

}  // namespace ash

#endif  // ASH_FRAME_CAPTION_BUTTONS_MAXIMIZE_BUBBLE_CONTROLLER_BUBBLE_H_
