// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BACKGROUND_ASH_USER_WALLPAPER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_BACKGROUND_ASH_USER_WALLPAPER_DELEGATE_H_

namespace ash {
class UserWallpaperDelegate;
}

namespace chromeos {

ash::UserWallpaperDelegate* CreateUserWallpaperDelegate();

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_BACKGROUND_ASH_USER_WALLPAPER_DELEGATE_H_
