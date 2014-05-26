// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

#include <string>

#include "base/basictypes.h"
#include "url/gurl.h"

namespace extension_urls {

// Returns the URL prefix for the extension/apps gallery. Can be set via the
// --apps-gallery-url switch. The URL returned will not contain a trailing
// slash. Do not use this as a prefix/extent for the store.
std::string GetWebstoreLaunchURL();

// Returns the URL to the extensions category on the Web Store. This is
// derived from GetWebstoreLaunchURL().
std::string GetExtensionGalleryURL();

// Returns the URL prefix for an item in the extension/app gallery. This URL
// will contain a trailing slash and should be concatenated with an item ID
// to get the item detail URL.
std::string GetWebstoreItemDetailURLPrefix();

// Returns the URL used to get webstore data (ratings, manifest, icon URL,
// etc.) about an extension from the webstore as JSON.
GURL GetWebstoreItemJsonDataURL(const std::string& extension_id);

// Returns the URL used to get webstore search results in JSON format. The URL
// returns a JSON dictionary that has the search results (under "results").
// Each entry in the array is a dictionary as the data returned for
// GetWebstoreItemJsonDataURL above. |query| is the user typed query string.
// |hl| is the host language code, e.g. en_US. Both arguments will be escaped
// and added as a query parameter to the returned web store json search URL.
GURL GetWebstoreJsonSearchUrl(const std::string& query, const std::string& hl);

// Returns the URL of the web store search results page for |query|.
GURL GetWebstoreSearchPageUrl(const std::string& query);

// Return the update URL used by gallery/webstore extensions/apps. This may
// have been overridden by a command line flag for testing purposes.
GURL GetWebstoreUpdateUrl();

// This returns the compile-time constant webstore update url. Usually you
// should prefer using GetWebstoreUpdateUrl.
GURL GetDefaultWebstoreUpdateUrl();

// Returns whether the URL is the webstore update URL (just considering host
// and path, not scheme, query, etc.)
bool IsWebstoreUpdateUrl(const GURL& update_url);

// Returns true if the URL points to an extension blacklist.
bool IsBlacklistUpdateUrl(const GURL& url);
// The greatest common prefixes of the main extensions gallery's browse and
// download URLs.
extern const char kGalleryBrowsePrefix[];

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

// The extension id of the Easy Unlock component application.
extern const char kEasyUnlockAppId[];

// The extension id of the Enterprise Web Store component application.
extern const char kEnterpriseWebStoreAppId[];

// The extension id of GMail application.
extern const char kGmailAppId[];

// The extension id of the Google Doc application.
extern const char kGoogleDocAppId[];

// The extension id of the Google Play Music application.
extern const char kGooglePlayMusicAppId[];

// The extension id of the Google Search application.
extern const char kGoogleSearchAppId[];

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

// The extension id of the hotword voice search trigger extension.
extern const char kHotwordExtensionId[];

// The extension id of the PDF extension.
extern const char kPdfExtensionId[];

// The extension id of the Office Viewer component extension.
extern const char kQuickOfficeComponentExtensionId[];

// The extension id of the Office Viewer extension on the internal webstore.
extern const char kQuickOfficeInternalExtensionId[];

// The extension id of the Office Viewer extension.
extern const char kQuickOfficeExtensionId[];

// The extension id of the settings application.
extern const char kSettingsAppId[];

// The extension id used for testing streamsPrivate
extern const char kStreamsPrivateTestExtensionId[];

// The extension id of the Web Store component application.
extern const char kWebStoreAppId[];

// The extension id of the Youtube application.
extern const char kYoutubeAppId[];

// The extension id of the in-app payments support application.
extern const char kInAppPaymentsSupportAppId[];

// The name of the app launch histogram.
extern const char kAppLaunchHistogram[];

// The name of the app launch histogram for platform apps.
extern const char kPlatformAppLaunchHistogram[];

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
// Name of ChromeVox manifest file.
extern const char kChromeVoxManifestFilename[];
// Name of ChromeVox guest manifest file.
extern const char kChromeVoxGuestManifestFilename[];
// Extension id, path (relative to |chrome::DIR_RESOURCES|) and IME engine
// id for the builtin-in Braille IME extension.
extern const char kBrailleImeExtensionId[];
extern const char kBrailleImeExtensionPath[];
extern const char kBrailleImeEngineId[];
// Path to preinstalled Connectivity Diagnostics extension.
extern const char kConnectivityDiagnosticsPath[];
extern const char kConnectivityDiagnosticsKioskPath[];
extern const char kConnectivityDiagnosticsLauncherPath[];
// Path to preinstalled speech synthesis extension.
extern const char kSpeechSynthesisExtensionPath[];
// The extension id of the speech synthesis extension.
extern const char kSpeechSynthesisExtensionId[];
// The extension id of the wallpaper manager application.
extern const char kWallpaperManagerId[];
// The extension id of the first run dialog application.
extern const char kFirstRunDialogId[];
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

// The key used for signing some pieces of data from the webstore.
extern const uint8 kWebstoreSignaturesPublicKey[];
extern const int kWebstoreSignaturesPublicKeySize;

}  // namespace extension_misc

namespace extensions {

// This enum is used for the launch type the user wants to use for an
// application.
// Do not remove items or re-order this enum as it is used in preferences
// and histograms.
enum LaunchType {
  LAUNCH_TYPE_INVALID = -1,
  LAUNCH_TYPE_FIRST = 0,
  LAUNCH_TYPE_PINNED = LAUNCH_TYPE_FIRST,
  LAUNCH_TYPE_REGULAR = 1,
  LAUNCH_TYPE_FULLSCREEN = 2,
  LAUNCH_TYPE_WINDOW = 3,
  NUM_LAUNCH_TYPES,

  // Launch an app in the in the way a click on the NTP would,
  // if no user pref were set.  Update this constant to change
  // the default for the NTP and chrome.management.launchApp().
  LAUNCH_TYPE_DEFAULT = LAUNCH_TYPE_REGULAR
};

// Don't remove items or change the order of this enum.  It's used in
// histograms and preferences.
enum LaunchContainer {
  LAUNCH_CONTAINER_WINDOW,
  LAUNCH_CONTAINER_PANEL,
  LAUNCH_CONTAINER_TAB,
  // For platform apps, which don't actually have a container (they just get a
  // "onLaunched" event).
  LAUNCH_CONTAINER_NONE
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
