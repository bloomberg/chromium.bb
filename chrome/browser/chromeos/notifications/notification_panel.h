// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#ifndef CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_
#define CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_

#include "base/gfx/rect.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/panel_controller.h"

class BalloonViewImpl;

namespace chromeos {

class BalloonContainer;

class NotificationPanel : PanelController::Delegate {
 public:
  // Returns the Singleton instance of NotificationPanel.
  static NotificationPanel* Get();

  // Adds/Removes a ballon view.
  void Add(BalloonViewImpl* view);
  void Remove(BalloonViewImpl* view);

  // Shows/Hides the Panel.
  void Show();
  void Hide();

  // PanelController overrides.
  virtual string16 GetPanelTitle();
  virtual SkBitmap GetPanelIcon();
  virtual void ClosePanel();

 private:
  friend struct DefaultSingletonTraits<NotificationPanel>;

  NotificationPanel();
  virtual ~NotificationPanel();

  void Init();
  // Returns the panel's bounds in the screen's coordinates.
  // The position will be controlled by window manager so
  // the origin is always (0, 0).
  gfx::Rect GetPanelBounds();

  BalloonContainer* balloon_container_;
  scoped_ptr<views::Widget> panel_widget_;
  scoped_ptr<PanelController> panel_controller_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NOTIFICATIONS_NOTIFICATION_PANEL_H_
