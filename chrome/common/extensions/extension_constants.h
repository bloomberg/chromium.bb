// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

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

  // Returns the URL leading to a search page for Web Intents. The search is
  // specific to intents with the given |action| and |type|.
  GURL GetWebstoreIntentQueryURL(const std::string& action,
                                 const std::string& type);

  // Returns the URL used to get webstore data (ratings, manifest, icon URL,
  // etc.) about an extension from the webstore as JSON.
  GURL GetWebstoreItemJsonDataURL(const std::string& extension_id);

  // Return the update URL used by gallery/webstore extensions/apps.
  GURL GetWebstoreUpdateUrl();

  // Returns whether the URL is the webstore update URL (just considering host
  // and path, not scheme, query, etc.)
  bool IsWebstoreUpdateUrl(const GURL& update_url);

  // Returns true if the URL points to an extension blacklist.
  bool IsBlacklistUpdateUrl(const GURL& url);

  // The greatest common prefixes of the main extensions gallery's browse and
  // download URLs.
  extern const char kGalleryBrowsePrefix[];
}  // namespace extension_urls

namespace extension_filenames {
  // The name of a temporary directory to install an extension into for
  // validation before finalizing install.
  extern const char kTempExtensionName[];

  // The file to write our decoded images to, relative to the extension_path.
  extern const char kDecodedImagesFilename[];

  // The file to write our decoded message catalogs to, relative to the
  // extension_path.
  extern const char kDecodedMessageCatalogsFilename[];

  // The filename to use for a background page generated from
  // background.scripts.
  extern const char kGeneratedBackgroundPageFilename[];
}

// Keys in the dictionary returned by Extension::GetBasicInfo().
namespace extension_info_keys {
  extern const char kDescriptionKey[];
  extern const char kEnabledKey[];
  extern const char kHomepageUrlKey[];
  extern const char kIdKey[];
  extern const char kNameKey[];
  extern const char kKioskEnabledKey[];
  extern const char kOfflineEnabledKey[];
  extern const char kOptionsUrlKey[];
  extern const char kDetailsUrlKey[];
  extern const char kVersionKey[];
  extern const char kPackagedAppKey[];
}

namespace extension_misc {
  // Matches chrome.windows.WINDOW_ID_NONE.
  const int kUnknownWindowId = -1;

  // Matches chrome.windows.WINDOW_ID_CURRENT.
  const int kCurrentWindowId = -2;

  // The extension id of the bookmark manager.
  extern const char kBookmarkManagerId[];

  // The extension id of the Citrix Receiver application.
  extern const char kCitrixReceiverAppId[];

  // The extension id of the beta Citrix Receiver application.
  extern const char kCitrixReceiverAppBetaId[];

  // The extension id of the dev Citrix Receiver application.
  extern const char kCitrixReceiverAppDevId[];

  // The extension id of the Enterprise Web Store component application.
  extern const char kEnterpriseWebStoreAppId[];

  // The extension id of the HTerm app for ChromeOS.
  extern const char kHTermAppId[];

  // The extension id of the HTerm dev app for ChromeOS.
  extern const char kHTermDevAppId[];

  // The extension id of the Crosh component app for ChromeOS.
  extern const char kCroshBuiltinAppId[];

  // The extension id of the Office Viewer component extension.
  extern const char kQuickOfficeComponentExtensionId[];

  // The extension id of the Office Viewer dev extension.
  extern const char kQuickOfficeDevExtensionId[];

  // The extension id of the Office Viewer extension.
  extern const char kQuickOfficeExtensionId[];

  // The extension id used for testing streamsPrivate
  extern const char kStreamsPrivateTestExtensionId[];

  // The extension id of the Web Store component application.
  extern const char kWebStoreAppId[];

  // The extension id of the Cloud Print component application.
  extern const char kCloudPrintAppId[];

  // The extension id of the Chrome component application.
  extern const char kChromeAppId[];

  // The extension id of the settings application.
  extern const char kSettingsAppId[];

  // Note: this structure is an ASN.1 which encodes the algorithm used
  // with its parameters. This is defined in PKCS #1 v2.1 (RFC 3447).
  // It is encoding: { OID sha1WithRSAEncryption      PARAMETERS NULL }
  const uint8 kSignatureAlgorithm[15] = {
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
    0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00
  };

  // Don't remove items or change the order of this enum.  It's used in
  // histograms and preferences.
  enum LaunchContainer {
    LAUNCH_WINDOW,
    LAUNCH_PANEL,
    LAUNCH_TAB,
    // For platform apps, which don't actually have a container (they just get a
    // "onLaunched" event).
    LAUNCH_NONE
  };

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

    // Launch using the --app or --app-id cmd line options.
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

    APP_LAUNCH_BUCKET_BOUNDARY,
    APP_LAUNCH_BUCKET_INVALID
  };

#if defined(OS_CHROMEOS)
  // Path to preinstalled ChromeVox screen reader extension.
  extern const char kChromeVoxExtensionPath[];
  // Path to preinstalled speech synthesis extension.
  extern const char kSpeechSynthesisExtensionPath[];
  // The extension id of the speech synthesis extension.
  extern const char kSpeechSynthesisExtensionId[];
  // The extension id of the wallpaper manager application.
  extern const char kWallpaperManagerId[];
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

  enum UnloadedExtensionReason {
    UNLOAD_REASON_DISABLE,    // Extension is being disabled.
    UNLOAD_REASON_UPDATE,     // Extension is being updated to a newer version.
    UNLOAD_REASON_UNINSTALL,  // Extension is being uninstalled.
    UNLOAD_REASON_TERMINATE,  // Extension has terminated.
    UNLOAD_REASON_BLACKLIST,  // Extension has been blacklisted.
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

  // NOTE: If you change this list, you should also change kExtensionIconSizes
  // in cc file.
  enum ExtensionIcons {
    EXTENSION_ICON_GIGANTOR = 512,
    EXTENSION_ICON_EXTRA_LARGE = 256,
    EXTENSION_ICON_LARGE = 128,
    EXTENSION_ICON_MEDIUM = 48,
    EXTENSION_ICON_SMALL = 32,
    EXTENSION_ICON_SMALLISH = 24,
    EXTENSION_ICON_ACTION = 19,
    EXTENSION_ICON_BITTY = 16,
    EXTENSION_ICON_INVALID = 0,
  };

  // List of sizes for extension icons that can be defined in the manifest.
  extern const int kExtensionIconSizes[];
  extern const size_t kNumExtensionIconSizes;

  // List of sizes for extension icons that can be defined in the manifest.
  extern const int kExtensionActionIconSizes[];
  extern const size_t kNumExtensionActionIconSizes;

  // List of sizes for extension icons that can be defined in the manifest.
  extern const int kScriptBadgeIconSizes[];
  extern const size_t kNumScriptBadgeIconSizes;

}  // extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
