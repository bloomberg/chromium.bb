// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_UI_GTK_NOTIFICATIONS_BALLOON_VIEW_GTK_H_
#define CHROME_BROWSER_UI_GTK_NOTIFICATIONS_BALLOON_VIEW_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "chrome/browser/ui/gtk/notifications/balloon_view_host_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

class BalloonCollection;
class CustomDrawButton;
class GtkThemeService;
class MenuGtk;
class NotificationDetails;
class NotificationOptionsMenuModel;
class NotificationSource;

namespace ui {
class SlideAnimation;
}

// A balloon view is the UI component for desktop notification toasts.
// It draws a border, and within the border an HTML renderer.
class BalloonViewImpl : public BalloonView,
                        public MenuGtk::Delegate,
                        public NotificationObserver,
                        public ui::AnimationDelegate {
 public:
  explicit BalloonViewImpl(BalloonCollection* collection);
  virtual ~BalloonViewImpl();

  // BalloonView interface.
  virtual void Show(Balloon* balloon);
  virtual void Update();
  virtual void RepositionToBalloon();
  virtual void Close(bool by_user);
  virtual gfx::Size GetSize() const;
  virtual BalloonHost* GetHost() const;

  // MenuGtk::Delegate interface.
  virtual void StoppedShowing();

 private:
  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ui::AnimationDelegate interface.
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Do the delayed close work.  The balloon and all view components will be
  // destroyed at this time, so it shouldn't be called while still processing
  // an event that relies on them.
  void DelayedClose(bool by_user);

  // The height of the balloon's shelf.
  // The shelf is where is close button is located.
  int GetShelfHeight() const;

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

  CHROMEGTK_CALLBACK_1(BalloonViewImpl, gboolean, OnContentsExpose,
                       GdkEventExpose*);
  CHROMEGTK_CALLBACK_0(BalloonViewImpl, void, OnCloseButton);
  CHROMEGTK_CALLBACK_1(BalloonViewImpl, gboolean, OnExpose, GdkEventExpose*);
  CHROMEGTK_CALLBACK_1(BalloonViewImpl, void, OnOptionsMenuButton,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_0(BalloonViewImpl, gboolean, OnDestroy);

  // Non-owned pointer to the balloon which owns this object.
  Balloon* balloon_;

  GtkThemeService* theme_service_;

  // The window that contains the frame of the notification.
  GtkWidget* frame_container_;

  // The widget that contains the shelf.
  GtkWidget* shelf_;

  // The hbox within the shelf that contains the buttons.
  GtkWidget* hbox_;

  // The window that contains the contents of the notification.
  GtkWidget* html_container_;

  // The renderer of the HTML contents.
  scoped_ptr<BalloonViewHost> html_contents_;

  // The following factory is used to call methods at a later time.
  ScopedRunnableMethodFactory<BalloonViewImpl> method_factory_;

  // Close button.
  scoped_ptr<CustomDrawButton> close_button_;

  // An animation to move the balloon on the screen as its position changes.
  scoped_ptr<ui::SlideAnimation> animation_;
  gfx::Rect anim_frame_start_;
  gfx::Rect anim_frame_end_;

  // The options menu.
  scoped_ptr<MenuGtk> options_menu_;
  scoped_ptr<NotificationOptionsMenuModel> options_menu_model_;
  // The button to open the options menu.
  scoped_ptr<CustomDrawButton> options_menu_button_;

  NotificationRegistrar notification_registrar_;

  // Is the menu currently showing?
  bool menu_showing_;

  // Is there a pending system-initiated close?
  bool pending_close_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

#endif  // CHROME_BROWSER_UI_GTK_NOTIFICATIONS_BALLOON_VIEW_GTK_H_
