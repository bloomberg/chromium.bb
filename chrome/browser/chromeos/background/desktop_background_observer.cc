// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/background/desktop_background_observer.h"

#include "ash/shell.h"
#include "ash/desktop_background/desktop_background_controller.h"
#include "ash/desktop_background/desktop_background_resources.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

namespace {

class UserWallpaperDelegate: public ash::UserWallpaperDelegate {
 public:
  UserWallpaperDelegate() {
  }

  virtual ~UserWallpaperDelegate() {
  }

  virtual const int GetUserWallpaperIndex() OVERRIDE {
    chromeos::UserManager* user_manager = chromeos::UserManager::Get();
    // If at login screen or logged in as a guest/incognito user, then use the
    // default wallpaper.
    if (user_manager->IsLoggedInAsGuest() || !user_manager->IsUserLoggedIn())
      return ash::GetGuestWallpaperIndex();

    const chromeos::User& user = user_manager->GetLoggedInUser();
    DCHECK(!user.email().empty());
    int index = user_manager->GetUserWallpaper(user.email());
    DCHECK(index >=0 && index < ash::GetWallpaperCount());
    return index;
  }

  virtual void OpenSetWallpaperPage() OVERRIDE {
    Browser* browser = Browser::GetOrCreateTabbedBrowser(
        ProfileManager::GetDefaultProfileOrOffTheRecord());
    browser->ShowOptionsTab("setWallpaper");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserWallpaperDelegate);
};

}  // namespace

ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() {
  return new chromeos::UserWallpaperDelegate();
}

DesktopBackgroundObserver::DesktopBackgroundObserver() {
  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_CHANGED,
      content::NotificationService::AllSources());
}

DesktopBackgroundObserver::~DesktopBackgroundObserver() {
}

void DesktopBackgroundObserver::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_CHANGED: {
      ash::Shell::GetInstance()->desktop_background_controller()->
          OnDesktopBackgroundChanged();
      break;
    }
    default:
      NOTREACHED();
  }
}

}  // namespace chromeos
