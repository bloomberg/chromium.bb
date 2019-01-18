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

// Enables dragging one or more tabs out of a browser window in tablet mode.
// TODO(xdai): Remove this after the feature is launched.
// https://crbug.com/823769.
ASH_PUBLIC_EXPORT extern const base::Feature kDragTabsInTabletMode;

// Enables notifications on the lock screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenNotifications;

// Enables inline reply on notifications on the lock screen.
// This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenInlineReply;

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature
    kLockScreenHideSensitiveNotificationsSupport;

// Enables the media session notification. If this is enabled, we will show
// a notification that shows the currently playing media with controls.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
ASH_PUBLIC_EXPORT extern const base::Feature kMediaSessionNotification;

// Enables the Night Light feature.
ASH_PUBLIC_EXPORT extern const base::Feature kNightLight;

// Enabled notification expansion animation.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationExpansionAnimation;

// Enables notification scroll bar in UnifiedSystemTray.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationScrollBar;

// Enables rounded corners for the Picture-in-picture window.
ASH_PUBLIC_EXPORT extern const base::Feature kPipRoundedCorners;

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
ASH_PUBLIC_EXPORT extern const base::Feature kSeparateNetworkIcons;

// Enables trilinear filtering.
ASH_PUBLIC_EXPORT extern const base::Feature kTrilinearFiltering;

// Enables running an external binary which provides lock screen authentication.
ASH_PUBLIC_EXPORT extern const base::Feature kUnlockWithExternalBinary;

// Enables the ContainedShell feature.
ASH_PUBLIC_EXPORT extern const base::Feature kContainedShell;

// Enables views login.
ASH_PUBLIC_EXPORT extern const base::Feature kViewsLogin;

// Enables using the BluetoothSystem Mojo interface for Bluetooth operations.
ASH_PUBLIC_EXPORT extern const base::Feature kUseBluetoothSystemInAsh;

// Enables the Supervised User Deprecation notices.
ASH_PUBLIC_EXPORT extern const base::Feature kSupervisedUserDeprecationNotice;

ASH_PUBLIC_EXPORT bool IsDockedMagnifierEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerAppEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenInlineReplyEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenHideSensitiveNotificationsSupported();

ASH_PUBLIC_EXPORT bool IsNightLightEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationExpansionAnimationEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationScrollBarEnabled();

ASH_PUBLIC_EXPORT bool IsPipRoundedCornersEnabled();

ASH_PUBLIC_EXPORT bool IsSeparateNetworkIconsEnabled();

ASH_PUBLIC_EXPORT bool IsTrilinearFilteringEnabled();

ASH_PUBLIC_EXPORT bool IsViewsLoginEnabled();

ASH_PUBLIC_EXPORT bool IsSupervisedUserDeprecationNoticeEnabled();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
