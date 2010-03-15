// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_

#include "base/task.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/frame/panel_controller.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#include "gfx/rect.h"

class Balloon;

namespace views {
class ScrollView;
}  // namespace views

namespace chromeos {

class BalloonContainer;
class BalloonViewImpl;

// NotificationPanel is a panel that displays notifications. It has
// several states and displays the different portion of notifications
// depending on in which state the panel is. The following shows
// how the panel's state changes in response to various events.
//
// Event List:
//   close: a user pressed close button on the title bar,
//          or the system closed the panel.
//   new : a new notification is added.
//   stale: one of new notifications became stale.
//   expand: a user pressed minimized panel to expand.
//   minimize: a user pressed the panel's title bar to minimize.
// For state, see State enum's description below.
//
//  CLOSE<--(event=close)-+     +--(event=stale, cond=has new|sticky)
//   |                    |     |         (event=new)
//   |                    |     V              |
//   +--(event=new)--> STICKY_AND_NEW----------+
//   |                  |           ^
//   |    (event=stale,             |
//   |    cond=has new, no sticy)  (event=new)
//   |       (event=minimize)       |
//   |                  |           |
//   |                  V           |
//   |                    MINIMIZED  ---(event=close)--> [CLOSE]
//   |                     |     ^
//   |                     |     |
//   |          (event=expand)  (event=minmize)
//   |                     V     |
//   +--(event=open)---->    FULL  --(event=stale)(event=new)
//                         |     |              |
//              (event=close)    +--------------+
//                         |
//          [CLOSE] <------+
//
class NotificationPanel : public PanelController::Delegate,
                          public BalloonCollectionImpl::NotificationUI {
 public:
  NotificationPanel();
  virtual ~NotificationPanel();

  // Shows/Hides the Panel.
  void Show();
  void Hide();

  // BalloonCollectionImpl::NotificationUI overrides..
  virtual void Add(Balloon* balloon);
  virtual void Remove(Balloon* balloon);
  virtual void ResizeNotification(Balloon* balloon,
                                  const gfx::Size& size);

  // PanelController overrides.
  virtual string16 GetPanelTitle();
  virtual SkBitmap GetPanelIcon();
  virtual void ClosePanel();
  virtual void OnPanelStateChanged(PanelController::State state);

 private:
  enum State {
    FULL,  // Show all notifications
    STICKY_AND_NEW,  // Show only new and sticky notifications.
    MINIMIZED,  // The panel is minimized.
    CLOSED,  // The panel is closed.
  };

  void Init();

  // Update the Panel Size according to its state.
  void UpdatePanel(bool contents_changed);

  // Returns the panel's preferred bounds in the screen's coordinates.
  // The position will be controlled by window manager so
  // the origin is always (0, 0).
  gfx::Rect GetPreferredBounds();

  // Returns the bounds that covers sticky and new notifications.
  gfx::Rect GetStickyNewBounds();

  void StartStaleTimer(Balloon* balloon);

  // A callback function that is called when the notification
  // (that the view is associated with) becomes stale after a timeout.
  void OnStale(BalloonViewImpl* view);

  BalloonContainer* balloon_container_;
  scoped_ptr<views::Widget> panel_widget_;
  scoped_ptr<PanelController> panel_controller_;
  scoped_ptr<views::ScrollView> scroll_view_;
  State state_;
  ScopedRunnableMethodFactory<NotificationPanel> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPanel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_
