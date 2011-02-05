// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "ui/gfx/path.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/view.h"

namespace views {
class Menu2;
class MenuButton;
class MouseEvent;
class TextButton;
class WidgetGtk;
}  // namespace views

class Notification;
class NotificationDetails;
class NotificationSource;

namespace chromeos {

class BalloonViewHost;
class NotificationControlView;

// A balloon view is the UI component for a notification panel.
class BalloonViewImpl : public BalloonView,
                        public views::View,
                        public NotificationObserver {
 public:
  BalloonViewImpl(bool sticky, bool controls, bool dom_ui);
  virtual ~BalloonViewImpl();

  // views::View interface.
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // BalloonView interface.
  virtual void Show(Balloon* balloon);
  virtual void Update();
  virtual void Close(bool by_user);
  virtual void RepositionToBalloon();
  gfx::Size GetSize() const;
  virtual BalloonHost* GetHost() const;

  // True if the notification is stale. False if the notification is new.
  bool stale() const { return stale_; }

  // Makes the notification stale.
  void set_stale() { stale_ = true; }

  // True if the notification is sticky.
  bool sticky() const { return sticky_; }

  // True if the notification is being closed.
  bool closed() const { return closed_; }

  // True if the balloon is for the given |notification|.
  bool IsFor(const Notification& notification) const;

  // Called when the notification becomes active (mouse is on).
  void Activated();

  // Called when the notification becomes inactive.
  void Deactivated();

 private:
  friend class NotificationControlView;

  // views::View interface.
  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(1000, 1000);
  }

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Initializes the options menu.
  void CreateOptionsMenu();

  // Do the delayed close work.
  void DelayedClose(bool by_user);

  // Denies the permission to show the ballooon from its source.
  void DenyPermission();

  // Returns the renderer's native view.
  gfx::NativeView GetParentNativeView();

  // Non-owned pointer to the balloon which owns this object.
  Balloon* balloon_;

  // The renderer of the HTML contents. Pointer owned by the views hierarchy.
  BalloonViewHost* html_contents_;

  // The following factory is used to call methods at a later time.
  ScopedRunnableMethodFactory<BalloonViewImpl> method_factory_;

  // A widget for ControlView.
  scoped_ptr<views::WidgetGtk> control_view_host_;

  bool stale_;
  NotificationRegistrar notification_registrar_;
  // A sticky flag. A sticky notification cannot be dismissed by a user.
  bool sticky_;
  // True if a notification should have info/option/dismiss label/buttons.
  bool controls_;
  // True if the notification is being closed.
  bool closed_;
  // True to enable domui in the notification.
  bool dom_ui_;

  DISALLOW_COPY_AND_ASSIGN(BalloonViewImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_BALLOON_VIEW_H_
