// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_FEATURES_H_
#define ASH_PUBLIC_CPP_ASH_FEATURES_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/feature_list.h"

namespace ash {
namespace features {

// Enables the UI to support Ambient EQ if the device supports it.
// See https://crbug.com/1021193 for more details.
ASH_PUBLIC_EXPORT extern const base::Feature kAllowAmbientEQ;

// Enables the Auto Night Light feature which sets the default schedule type to
// sunset-to-sunrise until the user changes it to something else. This feature
// is not exposed to the end user, and is enabled only via cros_config for
// certain devices.
ASH_PUBLIC_EXPORT extern const base::Feature kAutoNightLight;

// Enables contextual nudges for gesture education.
ASH_PUBLIC_EXPORT extern const base::Feature kContextualNudges;

// Enables shortcuts on corners of the display.
ASH_PUBLIC_EXPORT extern const base::Feature kCornerShortcuts;

// Enables indicators to hint where displays are connected.
ASH_PUBLIC_EXPORT extern const base::Feature kDisplayAlignAssist;

// Enables a modal dialog when resolution or refresh rate change.
ASH_PUBLIC_EXPORT extern const base::Feature kDisplayChangeModal;

// Enables identification overlays on each display.
ASH_PUBLIC_EXPORT extern const base::Feature kDisplayIdentification;

// Enables the docked (a.k.a. picture-in-picture) magnifier.
// TODO(afakhry): Remove this after the feature is fully launched.
// https://crbug.com/709824.
ASH_PUBLIC_EXPORT extern const base::Feature kDockedMagnifier;

// Enables dragging and snapping an overview window in clamshell mode.
// TODO(crbug.com/890029): Remove this when the feature is fully launched.
ASH_PUBLIC_EXPORT extern const base::Feature kDragToSnapInClamshellMode;

// Enables resizing/moving the selection region for partial screenshot.
ASH_PUBLIC_EXPORT extern const base::Feature kMovablePartialScreenshot;

// Enables rounded corners in overview mode for testing.
// TODO(crbug.com/903486): Remove this when new rounded corners implementation
// has landed.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableOverviewRoundedCorners;

// Limits the windows listed in Alt-Tab to the ones in the currently active
// desk.
ASH_PUBLIC_EXPORT extern const base::Feature kLimitAltTabToActiveDesk;

// Enables notifications on the lock screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenNotifications;

// Enables inline reply on notifications on the lock screen.
// This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenInlineReply;

// Supports the feature to hide sensitive content in notifications on the lock
// screen. This option is effective when |kLockScreenNotification| is enabled.
ASH_PUBLIC_EXPORT extern const base::Feature
    kLockScreenHideSensitiveNotificationsSupport;

// Enables lock screen media controls UI and use of media keys on the lock
// screen.
ASH_PUBLIC_EXPORT extern const base::Feature kLockScreenMediaControls;

// Enables hiding of ARC media notifications. If this is enabled, all ARC
// notifications that are of the media type will not be shown. This
// is because they will be replaced by native media session notifications.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
ASH_PUBLIC_EXPORT extern const base::Feature kHideArcMediaNotifications;

// Enables the redesigned managed device info UI in the system tray.
ASH_PUBLIC_EXPORT extern const base::Feature kManagedDeviceUIRedesign;

// Enables the media session notification. If this is enabled, we will show
// a notification that shows the currently playing media with controls.
// TODO(beccahughes): Remove after launch. (https://crbug.com/897836)
ASH_PUBLIC_EXPORT extern const base::Feature kMediaSessionNotification;

// Enables multi-display support for overview and split view.
// TODO(crbug.com/952461): Remove this when the feature is fully launched.
ASH_PUBLIC_EXPORT extern const base::Feature kMultiDisplayOverviewAndSplitView;

// Enables the Night Light feature.
ASH_PUBLIC_EXPORT extern const base::Feature kNightLight;

// Enabled notification expansion animation.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationExpansionAnimation;

// Shorten notification timeouts to 6 seconds.
ASH_PUBLIC_EXPORT extern const base::Feature
    kNotificationExperimentalShortTimeouts;

// Enables notification scroll bar in UnifiedSystemTray.
ASH_PUBLIC_EXPORT extern const base::Feature kNotificationScrollBar;

// Enables rounded corners for the Picture-in-picture window.
ASH_PUBLIC_EXPORT extern const base::Feature kPipRoundedCorners;

// Enables suppression of Displays notifications other than resolution change.
ASH_PUBLIC_EXPORT extern const base::Feature kReduceDisplayNotifications;

// Enables displaying separate network icons for different networks types.
// https://crbug.com/902409
ASH_PUBLIC_EXPORT extern const base::Feature kSeparateNetworkIcons;

// Enables trilinear filtering.
ASH_PUBLIC_EXPORT extern const base::Feature kTrilinearFiltering;

// Enables running an external binary which provides lock screen authentication.
ASH_PUBLIC_EXPORT extern const base::Feature kUnlockWithExternalBinary;

// Enables using the BluetoothSystem Mojo interface for Bluetooth operations.
ASH_PUBLIC_EXPORT extern const base::Feature kUseBluetoothSystemInAsh;

// Enables the Supervised User Deprecation notices.
ASH_PUBLIC_EXPORT extern const base::Feature kSupervisedUserDeprecationNotice;

// Enables side volume button control based on screen orientation feature.
// TODO(https://crbug.com/937907): Remove this after the feature is fully
// launched.
ASH_PUBLIC_EXPORT extern const base::Feature
    kSwapSideVolumeButtonsForOrientation;

// Enables shelf app scaling.
ASH_PUBLIC_EXPORT extern const base::Feature kShelfAppScaling;

// Enables background blur for the app list, shelf, unified system tray,
// autoclick menu, etc. Also enables the AppsGridView mask layer, slower devices
// may have choppier app list animations while in this mode. crbug.com/765292.
ASH_PUBLIC_EXPORT extern const base::Feature kEnableBackgroundBlur;

// Enables refactored UnifiedMessageCenter which is completely separated from
// the UnifiedSystemTrayView.
ASH_PUBLIC_EXPORT extern const base::Feature kUnifiedMessageCenterRefactor;

// Enables going back to previous page while swiping from the left edge of the
// display. Only for tablet mode.
ASH_PUBLIC_EXPORT extern const base::Feature kSwipingFromLeftEdgeToGoBack;

// Enables dragging from shelf to enter home screen or overview feature.
// Only for tablet mode.
// TODO(https://crbug.com/992642): Remove this after this feature is fully
// launched.
ASH_PUBLIC_EXPORT extern const base::Feature kDragFromShelfToHomeOrOverview;

// When enabled, shelf navigation controls and the overview tray item will be
// removed from the shelf in tablet mode (unless otherwise specified by user
// preferences, or policy).
ASH_PUBLIC_EXPORT extern const base::Feature kHideShelfControlsInTabletMode;

// Enables sliders for setting mic gain levels in the more audio settings
// section in the system tray.
ASH_PUBLIC_EXPORT extern const base::Feature kSystemTrayMicGainSetting;

// Enables special handling of Chrome tab drags from a WebUI tab strip.
// These will be treated similarly to a window drag, showing split view
// indicators in tablet mode, etc. The functionality is behind a flag
// right now since it is under development.
ASH_PUBLIC_EXPORT extern const base::Feature kWebUITabStripTabDragIntegration;

ASH_PUBLIC_EXPORT bool IsAllowAmbientEQEnabled();

ASH_PUBLIC_EXPORT bool IsAltTabLimitedToActiveDesk();

ASH_PUBLIC_EXPORT bool IsAutoNightLightEnabled();

ASH_PUBLIC_EXPORT bool IsHideArcMediaNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsKeyboardShortcutViewerAppEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsManagedDeviceUIRedesignEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenInlineReplyEnabled();

ASH_PUBLIC_EXPORT bool IsLockScreenHideSensitiveNotificationsSupported();

ASH_PUBLIC_EXPORT bool IsNotificationExpansionAnimationEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationExperimentalShortTimeoutsEnabled();

ASH_PUBLIC_EXPORT bool IsNotificationScrollBarEnabled();

ASH_PUBLIC_EXPORT bool IsPipRoundedCornersEnabled();

ASH_PUBLIC_EXPORT bool IsSeparateNetworkIconsEnabled();

ASH_PUBLIC_EXPORT bool IsTrilinearFilteringEnabled();

ASH_PUBLIC_EXPORT bool IsSupervisedUserDeprecationNoticeEnabled();

ASH_PUBLIC_EXPORT bool IsSwapSideVolumeButtonsForOrientationEnabled();

ASH_PUBLIC_EXPORT bool IsUnifiedMessageCenterRefactorEnabled();

ASH_PUBLIC_EXPORT bool IsBackgroundBlurEnabled();

ASH_PUBLIC_EXPORT bool IsSwipingFromLeftEdgeToGoBackEnabled();

ASH_PUBLIC_EXPORT bool IsDragFromShelfToHomeOrOverviewEnabled();

ASH_PUBLIC_EXPORT bool IsReduceDisplayNotificationsEnabled();

ASH_PUBLIC_EXPORT bool IsHideShelfControlsInTabletModeEnabled();

ASH_PUBLIC_EXPORT bool IsDisplayChangeModalEnabled();

ASH_PUBLIC_EXPORT bool AreContextualNudgesEnabled();

ASH_PUBLIC_EXPORT bool IsCornerShortcutsEnabled();

ASH_PUBLIC_EXPORT bool IsSystemTrayMicGainSettingEnabled();

ASH_PUBLIC_EXPORT bool IsDisplayIdentificationEnabled();

ASH_PUBLIC_EXPORT bool IsWebUITabStripTabDragIntegrationEnabled();

ASH_PUBLIC_EXPORT bool IsDisplayAlignmentAssistanceEnabled();

ASH_PUBLIC_EXPORT bool IsMovablePartialScreenshotEnabled();

ASH_PUBLIC_EXPORT bool IsAppScalingEnabled();

// These two functions are supposed to be temporary functions to set or get
// whether "WebUITabStrip" feature is enabled from Chrome.
ASH_PUBLIC_EXPORT void SetWebUITabStripEnabled(bool enabled);
ASH_PUBLIC_EXPORT bool IsWebUITabStripEnabled();

}  // namespace features
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_FEATURES_H_
