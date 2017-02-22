// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

#include <stdint.h>

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "url/gurl.h"

namespace extension_urls {

// This returns the compile-time constant webstore update url specific to
// Chrome. Usually you should prefer using GetWebstoreUpdateUrl.
const char* GetDefaultWebstoreUpdateUrl();

// Field to use with webstore URL for tracking launch source.
extern const char kWebstoreSourceField[];

// Values to use with webstore URL launch source field.
extern const char kLaunchSourceAppList[];
extern const char kLaunchSourceAppListSearch[];
extern const char kLaunchSourceAppListInfoDialog[];

}  // namespace extension_urls

namespace extension_misc {

// The extension id of the bookmark manager.
extern const char kBookmarkManagerId[];

// The extension id of the Chrome component application.
extern const char kChromeAppId[];

// The extension id of the Cloud Print component application.
extern const char kCloudPrintAppId[];

// The extension id of the Data Saver extension.
extern const char kDataSaverExtensionId[];

// The extension id of the Drive extension.
extern const char kDriveExtensionId[];

// The extension id of the Drive hosted app.
extern const char kDriveHostedAppId[];

// The extension id of the Easy Unlock component application.
extern const char kEasyUnlockAppId[];

// The extension id of the Enterprise Web Store component application.
extern const char kEnterpriseWebStoreAppId[];

// The extension id of the feedback component extension.
extern const char kFeedbackExtensionId[];

// The extension id of GMail application.
extern const char kGmailAppId[];

// The extension id of the Google Doc application.
extern const char kGoogleDocAppId[];

// The extension id of the Google Play Music application.
extern const char kGooglePlayMusicAppId[];

// The extension id of the Google Sheets application.
extern const char kGoogleSheetsAppId[];

// The extension id of the Google Slides application.
extern const char kGoogleSlidesAppId[];

// The extension id of the HTerm app for ChromeOS.
extern const char kHTermAppId[];

// The extension id of the HTerm dev app for ChromeOS.
extern const char kHTermDevAppId[];

// The extension id of the Identity API UI application.
extern const char kIdentityApiUiAppId[];

// The extension id of the Crosh component app for ChromeOS.
extern const char kCroshBuiltinAppId[];

// The extension id of the hotword audio verification dialogue app.
extern const char kHotwordAudioVerificationAppId[];

// The extension id of the new (experimental) hotword extension.
extern const char kHotwordNewExtensionId[];

// The extension id of the hotword shared module.
extern const char kHotwordSharedModuleId[];

// The extension id of the Youtube application.
extern const char kYoutubeAppId[];

// The extension id of the in-app payments support application.
extern const char kInAppPaymentsSupportAppId[];

#if defined(ENABLE_MEDIA_ROUTER)
// The extension id of the stable media router extension.
extern const char kMediaRouterStableExtensionId[];
#endif  // defined(ENABLE_MEDIA_ROUTER)

// The buckets used for app launches.
enum AppLaunchBucket {
  // Launch from NTP apps section while maximized.
  APP_LAUNCH_NTP_APPS_MAXIMIZED,

  // Launch from NTP apps section while collapsed.
  APP_LAUNCH_NTP_APPS_COLLAPSED,

  // Launch from NTP apps section while in menu mode.
  APP_LAUNCH_NTP_APPS_MENU,

  // Launch from NTP most visited section in any mode.
  APP_LAUNCH_NTP_MOST_VISITED,

  // Launch from NTP recently closed section in any mode.
  APP_LAUNCH_NTP_RECENTLY_CLOSED,

  // App link clicked from bookmark bar.
  APP_LAUNCH_BOOKMARK_BAR,

  // Nvigated to an app from within a web page (like by clicking a link).
  APP_LAUNCH_CONTENT_NAVIGATION,

  // Launch from session restore.
  APP_LAUNCH_SESSION_RESTORE,

  // Autolaunched at startup, like for pinned tabs.
  APP_LAUNCH_AUTOLAUNCH,

  // Launched from omnibox app links.
  APP_LAUNCH_OMNIBOX_APP,

  // App URL typed directly into the omnibox (w/ instant turned off).
  APP_LAUNCH_OMNIBOX_LOCATION,

  // Navigate to an app URL via instant.
  APP_LAUNCH_OMNIBOX_INSTANT,

  // Launch via chrome.management.launchApp.
  APP_LAUNCH_EXTENSION_API,

  // Launch an app via a shortcut. This includes using the --app or --app-id
  // command line arguments, or via an app shim process on Mac.
  APP_LAUNCH_CMD_LINE_APP,

  // App launch by passing the URL on the cmd line (not using app switches).
  APP_LAUNCH_CMD_LINE_URL,

  // User clicked web store launcher on NTP.
  APP_LAUNCH_NTP_WEBSTORE,

  // App launched after the user re-enabled it on the NTP.
  APP_LAUNCH_NTP_APP_RE_ENABLE,

  // URL launched using the --app cmd line option, but the URL does not
  // correspond to an installed app. These launches are left over from a
  // feature that let you make desktop shortcuts from the file menu.
  APP_LAUNCH_CMD_LINE_APP_LEGACY,

  // User clicked web store link on the NTP footer.
  APP_LAUNCH_NTP_WEBSTORE_FOOTER,

  // User clicked [+] icon in apps page.
  APP_LAUNCH_NTP_WEBSTORE_PLUS_ICON,

  // User clicked icon in app launcher main view.
  APP_LAUNCH_APP_LIST_MAIN,

  // User clicked app launcher search result.
  APP_LAUNCH_APP_LIST_SEARCH,

  // User clicked the chrome app icon from the app launcher's main view.
  APP_LAUNCH_APP_LIST_MAIN_CHROME,

  // User clicked the webstore icon from the app launcher's main view.
  APP_LAUNCH_APP_LIST_MAIN_WEBSTORE,

  // User clicked the chrome app icon from the app launcher's search view.
  APP_LAUNCH_APP_LIST_SEARCH_CHROME,

  // User clicked the webstore icon from the app launcher's search view.
  APP_LAUNCH_APP_LIST_SEARCH_WEBSTORE,
  APP_LAUNCH_BUCKET_BOUNDARY,
  APP_LAUNCH_BUCKET_INVALID
};

// The extension id of the ChromeVox extension.
extern const char kChromeVoxExtensionId[];

#if defined(OS_CHROMEOS)
// Path to preinstalled ChromeVox screen reader extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kChromeVoxExtensionPath[];
// The extension id of the Select-to-speak extension.
extern const char kSelectToSpeakExtensionId[];
// Path to preinstalled Select-to-speak extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kSelectToSpeakExtensionPath[];
// The extension id of the Switch access extension.
extern const char kSwitchAccessExtensionId[];
// Path to preinstalled Switch access extension (relative to
// |chrome::DIR_RESOURCES|).
extern const char kSwitchAccessExtensionPath[];
// Name of the manifest file in an extension when a special manifest is used
// for guest mode.
extern const char kGuestManifestFilename[];
// Extension id, path (relative to |chrome::DIR_RESOURCES|) and IME engine
// id for the builtin-in Braille IME extension.
extern const char kBrailleImeExtensionId[];
extern const char kBrailleImeExtensionPath[];
extern const char kBrailleImeEngineId[];
// Path to preinstalled Connectivity Diagnostics extension.
extern const char kConnectivityDiagnosticsPath[];
extern const char kConnectivityDiagnosticsLauncherPath[];
// The extension id of the first run dialog application.
extern const char kFirstRunDialogId[];
// Path to preinstalled speech synthesis extension.
extern const char kSpeechSynthesisExtensionPath[];
// The extension id of the speech synthesis extension.
extern const char kSpeechSynthesisExtensionId[];
// The extension id of the wallpaper manager application.
extern const char kWallpaperManagerId[];
// The app id of the webstore widget component app.
extern const char kWebstoreWidgetAppId[];
// The extension id of the new ZIP unpacker extension.
extern const char kZIPUnpackerExtensionId[];
#endif

// What causes an extension to be installed? Used in histograms, so don't
// change existing values.
enum CrxInstallCause {
  INSTALL_CAUSE_UNSET = 0,
  INSTALL_CAUSE_USER_DOWNLOAD,
  INSTALL_CAUSE_UPDATE,
  INSTALL_CAUSE_EXTERNAL_FILE,
  INSTALL_CAUSE_AUTOMATION,
  NUM_INSTALL_CAUSES
};

// The states that an app can be in, as reported by chrome.app.installState
// and chrome.app.runningState.
extern const char kAppStateNotInstalled[];
extern const char kAppStateInstalled[];
extern const char kAppStateDisabled[];
extern const char kAppStateRunning[];
extern const char kAppStateCannotRun[];
extern const char kAppStateReadyToRun[];

// The path part of the file system url used for media file systems.
extern const char kMediaFileSystemPathPart[];
}  // namespace extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
