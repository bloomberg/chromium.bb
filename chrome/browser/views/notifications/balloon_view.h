// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_

#include "app/gfx/path.h"
#include "app/menus/simple_menu_model.h"
#include "app/slide_animation.h"
#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

namespace views {
class ButtonListener;
class ImagePainter;
class TextButton;
class WidgetWin;
class Menu2;
}  // namespace views

class BalloonViewHost;
class NotificationDetails;
class NotificationSource;
class SlideAnimation;

// A balloon view is the UI component for a desktop notification toasts.
// It draws a border, and within the border an HTML renderer.
class BalloonViewImpl : public BalloonView,
                        public views::View,
                        public views::ViewMenuDelegate,
                        public menus::SimpleMenuModel::Delegate,
                        public NotificationObserver,
                        public AnimationDelegate {
 public:
  BalloonViewImpl();
  ~BalloonViewImpl();

  // BalloonView interface.
  void Show(Balloon* balloon);
  void RepositionToBalloon();
  void Close(bool by_user);
  gfx::Size GetSize() const;

 private:
  // views::View interface.
  virtual void Paint(gfx::Canvas* canvas);
  virtual void DidChangeBounds(const gfx::Rect& previous,
                               const gfx::Rect& current);
  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(1000, 1000);
  }

  // views::ViewMenuDelegate interface.
  void RunMenu(views::View* source, const gfx::Point& pt);

  // menus::SimpleMenuModel::Delegate interface.
  virtual bool IsCommandIdChecked(int command_id) const;
  virtual bool IsCommandIdEnabled(int command_id) const;
  virtual bool GetAcceleratorForCommandId(int command_id,
                                          menus::Accelerator* accelerator);
  virtual void ExecuteCommand(int command_id);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // AnimationDelegate interface.
  virtual void AnimationProgressed(const Animation* animation);

  // Launches the options menu at screen coordinates |pt|.
  void RunOptionsMenu(const gfx::Point& pt);

  // Initializes the options menu.
  void CreateOptionsMenu();

  // How to mask the balloon contents to fit within the frame.
  // Populates |path| with the outline.
  void GetContentsMask(const gfx::Rect& contents_rect, gfx::Path* path) const;

  // Adjust the contents window size to be appropriate for the frame.
  void SizeContentsWindow();

  // Do the delayed close work.
  void DelayedClose(bool by_user);

  // The height of the balloon's shelf.
  // The shelf is where is close button is located.
  int GetShelfHeight() const;

  // The width of the frame (not including any shadow).
  int GetFrameWidth() const;

  // The height of the frame (not including any shadow).
  int GetTotalFrameHeight() const;

  // The height of the part of the frame around the balloon.
  int GetBalloonFrameHeight() const;

  int GetTotalWidth() const;
  int GetTotalHeight() const;

  gfx::Rect GetCloseButtonBounds() const;
  gfx::Rect GetOptionsMenuBounds() const;
  gfx::Rect GetLabelBounds() const;

  // Where the balloon contents should be placed with respect to the top left
  // of the frame.
  gfx::Point GetContentsOffset() const;

  // Where the balloon contents should be in screen coordinates.
  gfx::Rect GetContentsRectangle() const;

  // Non-owned pointer to the balloon which owns this object.
  Balloon* balloon_;

  // The window that contains the frame of the notification.
  // Pointer owned by the View subclass.
  views::Widget* frame_container_;

  // The window that contains the contents of the notification.
  // Pointer owned by the View subclass.
  views::Widget* html_container_;

  // The renderer of the HTML contents. Pointer owned by the views hierarchy.
  BalloonViewHost* html_contents_;

  // The following factory is used to call methods at a later time.
  ScopedRunnableMethodFactory<BalloonViewImpl> method_factory_;

  // Image painters for the frame of the toast.
  scoped_ptr<views::Painter> shelf_background_;
  scoped_ptr<views::Painter> balloon_background_;

  // Pointer to sub-view is owned by the View sub-class.
  views::TextButton* close_button_;

  // Pointer to sub-view is owned by View class.
  views::Label* source_label_;

  // Listener for clicks on the close button.
  scoped_ptr<views::ButtonListener> close_button_listener_;

  // An animation to move the balloon on the screen as its position changes.
  scoped_ptr<SlideAnimation> animation_;
  gfx::Rect anim_frame_start_;
  gfx::Rect anim_frame_end_;

  // The options menu.
  scoped_ptr<menus::SimpleMenuModel> options_menu_contents_;
  scoped_ptr<views::Menu2> options_menu_menu_;
  views::MenuButton* options_menu_button_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

#endif  // CHROME_BROWSER_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
