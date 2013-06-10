// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_
#define CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"

class Notification;

namespace gfx {
class ImageSkia;
}

// Provides the notification balloon functionality by using desktop
// notifications to platforms that don't have a specific native API.
class DesktopNotificationBalloon {
 public:
  DesktopNotificationBalloon();
  virtual ~DesktopNotificationBalloon();

  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const string16& title,
                      const string16& contents);

 private:
  std::string notification_id_;

  // Counter to provide unique ids to notifications.
  static int id_count_;

  DISALLOW_COPY_AND_ASSIGN(DesktopNotificationBalloon);
};

#endif  // CHROME_BROWSER_STATUS_ICONS_DESKTOP_NOTIFICATION_BALLOON_H_
