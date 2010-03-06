// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/frame/panel_controller.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"

class Balloon;

namespace views {
class ScrollView;
}  // namespace views

namespace chromeos {

class BalloonContainer;

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

 private:
  void Init();

  // Update the Panel Size to the preferred size.
  void UpdateSize();

  // Returns the panel's preferred bounds in the screen's coordinates.
  // The position will be controlled by window manager so
  // the origin is always (0, 0).
  gfx::Rect GetPreferredBounds();

  BalloonContainer* balloon_container_;
  scoped_ptr<views::Widget> panel_widget_;
  scoped_ptr<PanelController> panel_controller_;
  scoped_ptr<views::ScrollView> scroll_view_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPanel);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_
