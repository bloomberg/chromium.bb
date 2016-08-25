// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_BACKGROUND_ASH_WALLPAPER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_BACKGROUND_ASH_WALLPAPER_DELEGATE_H_

namespace ash {
class WallpaperDelegate;
}

namespace chromeos {

ash::WallpaperDelegate* CreateWallpaperDelegate();

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BACKGROUND_ASH_WALLPAPER_DELEGATE_H_
