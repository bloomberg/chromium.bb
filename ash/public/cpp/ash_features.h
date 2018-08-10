// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_FEATURES_H_
#define ASH_PUBLIC_CPP_ASH_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"

namespace ash {
namespace features {

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
ASH_PUBLIC_EXPORT extern const base::Feature kDockedMagnifier;

// Enables dragging an app window when it is in tablet mode.
// TODO(minch): Remove this after the feature is launched.
// https://crbug.com/847587.
ASH_PUBLIC_EXPORT extern const base::Feature kDragAppsInTabletMode;

// Enables dragging one or more tabs out of a browser window in tablet mode.
// TODO(xdai): Remove this after the feature is launched.
// https://crbug.com/823769.
ASH_PUBLIC_EXPORT extern const base::Feature kDragTabsInTabletMode;

// Enables the keyboard shortcut viewer.
// TODO(wutao): Remove this after the feature is fully launched.
// https://crbug.com/755448.
ASH_PUBLIC_EXPORT extern const base::Feature kKeyboardShortcutViewer;

// Enables the keyboard shortcut viewer mojo app.
// TODO(msw): Remove this after the feature is fully launched.
// https://crbug.com/841020.
ASH_PUBLIC_EXPORT extern const base::Feature kKeyboardShortcutViewerApp;

// Enables notifications on the lock screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenNotifications;

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature
    kLockScreenHideSensitiveNotificationsSupport;

// Enables the new wallpaper picker.
// TODO(wzang): Remove this after the feature is fully launched.
// https://crbug.com/777293.
ASH_PUBLIC_EXPORT extern const base::Feature kNewWallpaperPicker;

// Enables the Night Light feature.
ASH_PUBLIC_EXPORT extern const base::Feature kNightLight;

// Enables notification scroll bar in UnifiedSystemTray.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationScrollBar;

// Enables swipe to close in overview mode.
// TODO(sammiequon): Remove this after the feature is fully launched.
// https://crbug.com/828646.
ASH_PUBLIC_EXPORT extern const base::Feature kOverviewSwipeToClose;

// Enables new system menu.
ASH_PUBLIC_EXPORT extern const base::Feature kSystemTrayUnified;

// Enables trilinear filtering.
ASH_PUBLIC_EXPORT extern const base::Feature kTrilinearFiltering;

// Enables views login.
ASH_PUBLIC_EXPORT extern const base::Feature kViewsLogin;

ASH_PUBLIC_EXPORT bool IsDockedMagnifierEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerAppEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenHideSensitiveNotificationsSupported();

ASH_PUBLIC_EXPORT bool IsNewWallpaperPickerEnabled();

ASH_PUBLIC_EXPORT bool IsNightLightEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationScrollBarEnabled();

ASH_PUBLIC_EXPORT bool IsSystemTrayUnifiedEnabled();

ASH_PUBLIC_EXPORT bool IsTrilinearFilteringEnabled();

ASH_PUBLIC_EXPORT bool IsViewsLoginEnabled();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
