// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"
#include "chrome/common/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/path.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/label.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"
#include "views/widget/widget_delegate.h"

namespace views {
class ButtonListener;
class ImageButton;
class ImagePainter;
class TextButton;
class WidgetWin;
class Menu2;
}  // namespace views

class BalloonCollection;
class NotificationDetails;
class NotificationOptionsMenuModel;
class NotificationSource;

namespace ui {
class SlideAnimation;
}

// A balloon view is the UI component for a desktop notification toasts.
// It draws a border, and within the border an HTML renderer.
class BalloonViewImpl : public BalloonView,
                        public views::View,
                        public views::ViewMenuDelegate,
                        public views::WidgetDelegate,
                        public views::ButtonListener,
                        public NotificationObserver,
                        public ui::AnimationDelegate {
 public:
  explicit BalloonViewImpl(BalloonCollection* collection);
  ~BalloonViewImpl();

  // BalloonView interface.
  virtual void Show(Balloon* balloon);
  virtual void Update();
  virtual void RepositionToBalloon();
  virtual void Close(bool by_user);
  virtual gfx::Size GetSize() const;
  virtual BalloonHost* GetHost() const;

 private:
  // views::View interface.
  virtual void Paint(gfx::Canvas* canvas);
  virtual void OnBoundsChanged();
  virtual gfx::Size GetPreferredSize();

  // views::ViewMenuDelegate interface.
  virtual void RunMenu(views::View* source, const gfx::Point& pt);

  // views::WidgetDelegate interface.
  virtual void DisplayChanged();
  virtual void WorkAreaChanged();

  // views::ButtonListener interface.
  virtual void ButtonPressed(views::Button* sender, const views::Event&);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // ui::AnimationDelegate interface.
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Launches the options menu at screen coordinates |pt|.
  void RunOptionsMenu(const gfx::Point& pt);

  // Initializes the options menu.
  void CreateOptionsMenu();

  // Masks the contents to fit within the frame.
  void GetContentsMask(const gfx::Rect& contents_rect, gfx::Path* path) const;

  // Masks the frame for the rounded corners of the shadow-bubble.
  void GetFrameMask(const gfx::Rect&, gfx::Path* path) const;

  // Adjust the contents window size to be appropriate for the frame.
  void SizeContentsWindow();

  // Do the delayed close work.
  void DelayedClose(bool by_user);

  // The height of the balloon's shelf.
  // The shelf is where is close button is located.
  int GetShelfHeight() const;

  // The height of the part of the frame around the balloon.
  int GetBalloonFrameHeight() const;

  int GetTotalWidth() const;
  int GetTotalHeight() const;

  gfx::Rect GetCloseButtonBounds() const;
  gfx::Rect GetOptionsButtonBounds() const;
  gfx::Rect GetLabelBounds() const;

  // Where the balloon contents should be placed with respect to the top left
  // of the frame.
  gfx::Point GetContentsOffset() const;

  // Where the balloon contents should be in screen coordinates.
  gfx::Rect GetContentsRectangle() const;

  // Non-owned pointer to the balloon which owns this object.
  Balloon* balloon_;

  // Non-owned pointer to the balloon collection this is a part of.
  BalloonCollection* collection_;

  // The window that contains the frame of the notification.
  // Pointer owned by the View subclass.
  views::Widget* frame_container_;

  // The window that contains the contents of the notification.
  // Pointer owned by the View subclass.
  views::Widget* html_container_;

  // The renderer of the HTML contents.
  scoped_ptr<BalloonViewHost> html_contents_;

  // The following factory is used to call methods at a later time.
  ScopedRunnableMethodFactory<BalloonViewImpl> method_factory_;

  // Pointer to sub-view is owned by the View sub-class.
  views::ImageButton* close_button_;

  // Pointer to sub-view is owned by View class.
  views::Label* source_label_;

  // An animation to move the balloon on the screen as its position changes.
  scoped_ptr<ui::SlideAnimation> animation_;
  gfx::Rect anim_frame_start_;
  gfx::Rect anim_frame_end_;

  // The options menu.
  scoped_ptr<NotificationOptionsMenuModel> options_menu_model_;
  scoped_ptr<views::Menu2> options_menu_menu_;
  views::MenuButton* options_menu_button_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
