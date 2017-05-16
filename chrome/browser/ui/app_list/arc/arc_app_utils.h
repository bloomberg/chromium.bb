// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_

#include <string>

#include "base/callback.h"
#include "components/arc/common/app.mojom.h"
#include "ui/gfx/geometry/rect.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace arc {

extern const char kPlayStoreAppId[];
// This represents legacy Play Store item in the app launcher that was used
// before unifying app id in the app launcher and shelf.
// TODO(khmel): Remove this after few release http://crbug.com/722675.
extern const char kLegacyPlayStoreAppId[];
extern const char kPlayStorePackage[];
extern const char kPlayStoreActivity[];
extern const char kSettingsAppId[];

using CanHandleResolutionCallback = base::Callback<void(bool)>;

// Checks if a given app should be hidden in launcher.
bool ShouldShowInLauncher(const std::string& app_id);

// Launch an app and let the system decides how big and where to place it.
bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               int event_flags);

// Launch Android Settings app.
bool LaunchAndroidSettingsApp(content::BrowserContext* context,
                              int event_flags);

// Launch Play Store app.
bool LaunchPlayStoreWithUrl(const std::string& url);

// Launch an app with given layout and let the system decides how big and where
// to place it.
// TODO (khmel) replace bool for |landscape_layout| with enum class in order
// to prevent using another LaunchApp with different signature mistakenly.
bool LaunchApp(content::BrowserContext* context,
               const std::string& app_id,
               bool landscape_layout,
               int event_flags);

// Launch an app and place it at the specified coordinates.
bool LaunchAppWithRect(content::BrowserContext* context,
                       const std::string& app_id,
                       const gfx::Rect& target_rect,
                       int event_flags);

// Sets task active.
void SetTaskActive(int task_id);

// Closes the task.
void CloseTask(int task_id);

// Open TalkBack settings window.
void ShowTalkBackSettings();

// Tests if the application can use the given target resolution.
// The callback will receive the information once known.
// A false will get returned if the result cannot be determined in which case
// the callback will not be called.
bool CanHandleResolution(content::BrowserContext* context,
                         const std::string& app_id,
                         const gfx::Rect& rect,
                         const CanHandleResolutionCallback& callback);

// Uninstalls the package in ARC.
void UninstallPackage(const std::string& package_name);

// Uninstalls ARC app or removes shortcut.
void UninstallArcApp(const std::string& app_id, Profile* profile);

// Removes cached app shortcut icon in ARC.
void RemoveCachedIcon(const std::string& icon_resource_id);

// Shows package info for ARC package.
// Deprecated. Use ShowPackageInfoOnPage.
bool ShowPackageInfo(const std::string& package_name);

// Shows package info for ARC package at the specified page.
bool ShowPackageInfoOnPage(const std::string& package_name,
                           mojom::ShowPackageInfoPage page);

// Returns true if |id| represents either ARC app or ARC shelf group.
bool IsArcItem(content::BrowserContext* context, const std::string& id);

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_APP_UTILS_H_
