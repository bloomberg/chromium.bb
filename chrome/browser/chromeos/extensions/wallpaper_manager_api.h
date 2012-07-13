// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_MANAGER_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_MANAGER_API_H_

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "chrome/browser/extensions/extension_function.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_fetcher_delegate.h"

extern const char kWallpaperManagerDomain[];

// Wallpaper manager API functions. Followup CL will add implementations of
// some API functions.
namespace wallpaper_manager_util {

// Opens wallpaper manager application.
void OpenWallpaperManager();

}  // namespace wallpaper_manager_util

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_WALLPAPER_MANAGER_API_H_
