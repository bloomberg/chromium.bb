// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_GTK_NOTIFICATIONS_BALLOON_VIEW_GTK_H_
#define CHROME_BROWSER_GTK_NOTIFICATIONS_BALLOON_VIEW_GTK_H_

#include "app/animation.h"
#include "base/basictypes.h"
#include "base/gfx/point.h"
#include "base/gfx/rect.h"
#include "base/gfx/size.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"


class BalloonViewHost;
class MenuGtk;
class NineBox;
class NotificationDetails;
class NotificationOptionsMenuModel;
class NotificationSource;
class SlideAnimation;

// A balloon view is the UI component for desktop notification toasts.
// It draws a border, and within the border an HTML renderer.
class BalloonViewImpl : public BalloonView,
                        public MenuGtk::Delegate,
                        public NotificationObserver,
                        public AnimationDelegate {
 public:
  BalloonViewImpl();
  ~BalloonViewImpl();

  // BalloonView interface.
  virtual void Show(Balloon* balloon);
  virtual void RepositionToBalloon();
  virtual void Close(bool by_user);
  virtual gfx::Size GetSize() const;

 private:
  // MenuGtk::Delegate interface.  These methods shouldn't actually be
  // called because we are using a MenuModel which handles these callbacks.
  virtual bool IsCommandEnabled(int command_id) const {
    NOTIMPLEMENTED();
    return true;
  }
  virtual void ExecuteCommand(int command_id) {
    NOTIMPLEMENTED();
  }

  // Initializes the toolbar style with GTK.
  void InitToolbarStyle();

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // AnimationDelegate interface.
  virtual void AnimationProgressed(const Animation* animation);

  // Launches the options menu.
  void RunOptionsMenu();

  // Adjust the contents window size to be appropriate for the frame.
  void SizeContentsWindow();

  // Do the delayed close work.
  void DelayedClose(bool by_user);

  // The height of the balloon's shelf.
  // The shelf is where is close button is located.
  int GetShelfHeight() const;

  // The height of the part of the frame around the balloon.
  int GetBalloonFrameHeight() const;

  // The width and height that the frame should be.  If the balloon inside
  // changes size, this will not be the same as the actual frame size until
  // RepositionToBalloon() has been called and the animation completes.
  int GetDesiredTotalWidth() const;
  int GetDesiredTotalHeight() const;

  // Where the balloon contents should be placed with respect to the top left
  // of the frame.
  gfx::Point GetContentsOffset() const;

  // Where the balloon contents should be in screen coordinates.
  gfx::Rect GetContentsRectangle() const;

  static gboolean HandleExposeThunk(GtkWidget* widget,
                                    GdkEventExpose* event,
                                    gpointer user_data) {
    return reinterpret_cast<BalloonViewImpl*>(user_data)->HandleExpose();
  }
  gboolean HandleExpose();

  static void HandleCloseButtonThunk(GtkWidget* widget, gpointer user_data) {
    reinterpret_cast<BalloonViewImpl*>(user_data)->Close(true);
  }

  static void HandleOptionsMenuButtonThunk(GtkWidget* widget,
                                           gpointer user_data) {
    reinterpret_cast<BalloonViewImpl*>(user_data)->RunOptionsMenu();
  }

  // Non-owned pointer to the balloon which owns this object.
  Balloon* balloon_;

  // The window that contains the frame of the notification.
  GtkWidget* frame_container_;

  // The widget that contains the shelf.
  GtkWidget* shelf_;

  // The toolbar widget within the shelf that contains the buttons.
  GtkWidget* toolbar_;

  // The window that contains the contents of the notification.
  GtkWidget* html_container_;

  // The renderer of the HTML contents. Pointer owned by the views hierarchy.
  BalloonViewHost* html_contents_;

  // The following factory is used to call methods at a later time.
  ScopedRunnableMethodFactory<BalloonViewImpl> method_factory_;

  // Image painters for the frame of the toast.
  scoped_ptr<NineBox> shelf_background_;
  scoped_ptr<NineBox> balloon_background_;

  // Pointer to sub-view is owned by the View sub-class.
  GtkWidget* close_button_;

  // An animation to move the balloon on the screen as its position changes.
  scoped_ptr<SlideAnimation> animation_;
  gfx::Rect anim_frame_start_;
  gfx::Rect anim_frame_end_;

  // The options menu.
  scoped_ptr<MenuGtk> options_menu_;
  scoped_ptr<NotificationOptionsMenuModel> options_menu_model_;
  GtkWidget* options_menu_button_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

#endif  // CHROME_BROWSER_GTK_NOTIFICATIONS_BALLOON_VIEW_GTK_H_
