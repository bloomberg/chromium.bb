// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/ui/views/notifications/balloon_view_host.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/gfx/path.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/widget/widget_delegate.h"
#include "views/controls/label.h"
#include "views/controls/menu/view_menu_delegate.h"
#include "views/view.h"

class BalloonCollection;
class NotificationOptionsMenuModel;

namespace ui {
class SlideAnimation;
}

namespace views {
class ButtonListener;
class ImageButton;
class MenuRunner;
}

// A balloon view is the UI component for a desktop notification toasts.
// It draws a border, and within the border an HTML renderer.
class BalloonViewImpl : public BalloonView,
                        public views::ViewMenuDelegate,
                        public views::WidgetDelegateView,
                        public views::ButtonListener,
                        public content::NotificationObserver,
                        public ui::AnimationDelegate {
 public:
  explicit BalloonViewImpl(BalloonCollection* collection);
  virtual ~BalloonViewImpl();

  // BalloonView interface.
  virtual void Show(Balloon* balloon) OVERRIDE;
  virtual void Update() OVERRIDE;
  virtual void RepositionToBalloon() OVERRIDE;
  virtual void Close(bool by_user) OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual BalloonHost* GetHost() const OVERRIDE;

 private:
  // views::View interface.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::ViewMenuDelegate interface.
  virtual void RunMenu(views::View* source, const gfx::Point& pt) OVERRIDE;

  // views::WidgetDelegate interface.
  virtual void OnDisplayChanged() OVERRIDE;
  virtual void OnWorkAreaChanged() OVERRIDE;

  // views::ButtonListener interface.
  virtual void ButtonPressed(
      views::Button* sender, const views::Event&) OVERRIDE;

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ui::AnimationDelegate interface.
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

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
  base::WeakPtrFactory<BalloonViewImpl> method_factory_;

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
  scoped_ptr<views::MenuRunner> menu_runner_;
  views::MenuButton* options_menu_button_;

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

#endif  // CHROME_BROWSER_UI_VIEWS_NOTIFICATIONS_BALLOON_VIEW_H_
