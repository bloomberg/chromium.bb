// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BACKGROUND_DESKTOP_BACKGROUND_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_BACKGROUND_DESKTOP_BACKGROUND_OBSERVER_H_
#pragma once

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace chromeos {

// Listens for background change events and passes them to the Aura shell's
// DesktopBackgroundController class.
class DesktopBackgroundObserver : public content::NotificationObserver{
 public:
  DesktopBackgroundObserver();
  virtual ~DesktopBackgroundObserver();

 private:
  // Returns the index of user selected wallpaper from a default wallpaper list.
  int GetUserWallpaperIndex();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundObserver);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_BACKGROUND_DESKTOP_BACKGROUND_OBSERVER_H_
