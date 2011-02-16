// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#pragma once

#include "base/basictypes.h"

// Keys used in JSON representation of extensions.
namespace extension_manifest_keys {
  extern const char* kAllFrames;
  extern const char* kApp;
  extern const char* kBackground;
  extern const char* kBrowserAction;
  extern const char* kBrowseURLs;
  extern const char* kChromeURLOverrides;
  extern const char* kContentScripts;
  extern const char* kConvertedFromUserScript;
  extern const char* kCss;
  extern const char* kCurrentLocale;
  extern const char* kDefaultLocale;
  extern const char* kDescription;
  extern const char* kDevToolsPage;
  extern const char* kExcludeGlobs;
  extern const char* kHomepageURL;
  extern const char* kIcons;
  extern const char* kIncognito;
  extern const char* kIncludeGlobs;
  extern const char* kJs;
  extern const char* kLaunch;
  extern const char* kLaunchContainer;
  extern const char* kLaunchHeight;
  extern const char* kLaunchLocalPath;
  extern const char* kLaunchWebURL;
  extern const char* kLaunchWidth;
  extern const char* kMatches;
  extern const char* kMinimumChromeVersion;
  extern const char* kName;
  extern const char* kOmnibox;
  extern const char* kOmniboxKeyword;
  extern const char* kOptionsPage;
  extern const char* kPageAction;
  extern const char* kPageActionDefaultIcon;
  extern const char* kPageActionDefaultPopup;
  extern const char* kPageActionDefaultTitle;
  extern const char* kPageActionIcons;
  extern const char* kPageActionId;
  extern const char* kPageActionPopup;
  extern const char* kPageActionPopupHeight;
  extern const char* kPageActionPopupPath;
  extern const char* kPageActions;
  extern const char* kPermissions;
  extern const char* kPlugins;
  extern const char* kPluginsPath;
  extern const char* kPluginsPublic;
  extern const char* kPublicKey;
  extern const char* kRunAt;
  extern const char* kSidebar;
  extern const char* kSidebarDefaultIcon;
  extern const char* kSidebarDefaultPage;
  extern const char* kSidebarDefaultTitle;
  extern const char* kSignature;
  extern const char* kTheme;
  extern const char* kThemeColors;
  extern const char* kThemeDisplayProperties;
  extern const char* kThemeImages;
  extern const char* kThemeTints;
  extern const char* kToolstripPath;
  extern const char* kToolstrips;
  extern const char* kTts;
  extern const char* kTtsGenderFemale;
  extern const char* kTtsGenderMale;
  extern const char* kTtsVoices;
  extern const char* kTtsVoicesGender;
  extern const char* kTtsVoicesLocale;
  extern const char* kTtsVoicesVoiceName;
  extern const char* kType;
  extern const char* kUpdateURL;
  extern const char* kVersion;
  extern const char* kWebURLs;
}  // namespace extension_manifest_keys

// Some values expected in manifests.
namespace extension_manifest_values {
  extern const char* kIncognitoSplit;
  extern const char* kIncognitoSpanning;
  extern const char* kLaunchContainerPanel;
  extern const char* kLaunchContainerTab;
  extern const char* kLaunchContainerWindow;
  extern const char* kPageActionTypePermanent;
  extern const char* kPageActionTypeTab;
  extern const char* kRunAtDocumentEnd;
  extern const char* kRunAtDocumentIdle;
  extern const char* kRunAtDocumentStart;
}  // namespace extension_manifest_values

// Error messages returned from Extension::InitFromValue().
namespace extension_manifest_errors {
  extern const char* kAppsNotEnabled;
  extern const char* kCannotAccessPage;
  extern const char* kCannotScriptGallery;
  extern const char* kChromeVersionTooLow;
  extern const char* kDevToolsExperimental;
  extern const char* kDisabledByPolicy;
  extern const char* kExperimentalFlagRequired;
  extern const char* kHostedAppsCannotIncludeExtensionFeatures;
  extern const char* kInvalidAllFrames;
  extern const char* kInvalidBackground;
  extern const char* kInvalidBrowserAction;
  extern const char* kInvalidBrowseURL;
  extern const char* kInvalidBrowseURLs;
  extern const char* kInvalidChromeURLOverrides;
  extern const char* kInvalidContentScript;
  extern const char* kInvalidContentScriptsList;
  extern const char* kInvalidCss;
  extern const char* kInvalidCssList;
  extern const char* kInvalidDefaultLocale;
  extern const char* kInvalidDescription;
  extern const char* kInvalidDevToolsPage;
  extern const char* kInvalidGlob;
  extern const char* kInvalidGlobList;
  extern const char* kInvalidHomepageURL;
  extern const char* kInvalidIconPath;
  extern const char* kInvalidIcons;
  extern const char* kInvalidIncognitoBehavior;
  extern const char* kInvalidJs;
  extern const char* kInvalidJsList;
  extern const char* kInvalidKey;
  extern const char* kInvalidLaunchContainer;
  extern const char* kInvalidLaunchHeight;
  extern const char* kInvalidLaunchHeightContainer;
  extern const char* kInvalidLaunchLocalPath;
  extern const char* kInvalidLaunchWebURL;
  extern const char* kInvalidLaunchWidth;
  extern const char* kInvalidLaunchWidthContainer;
  extern const char* kInvalidManifest;
  extern const char* kInvalidMatch;
  extern const char* kInvalidMatchCount;
  extern const char* kInvalidMatches;
  extern const char* kInvalidMinimumChromeVersion;
  extern const char* kInvalidName;
  extern const char* kInvalidOmniboxKeyword;
  extern const char* kInvalidOptionsPage;
  extern const char* kInvalidOptionsPageExpectUrlInPackage;
  extern const char* kInvalidOptionsPageInHostedApp;
  extern const char* kInvalidPageAction;
  extern const char* kInvalidPageActionDefaultTitle;
  extern const char* kInvalidPageActionIconPath;
  extern const char* kInvalidPageActionId;
  extern const char* kInvalidPageActionName;
  extern const char* kInvalidPageActionOldAndNewKeys;
  extern const char* kInvalidPageActionPopup;
  extern const char* kInvalidPageActionPopupHeight;
  extern const char* kInvalidPageActionPopupPath;
  extern const char* kInvalidPageActionsList;
  extern const char* kInvalidPageActionsListSize;
  extern const char* kInvalidPageActionTypeValue;
  extern const char* kInvalidPermission;
  extern const char* kInvalidPermissions;
  extern const char* kInvalidPermissionScheme;
  extern const char* kInvalidPlugins;
  extern const char* kInvalidPluginsPath;
  extern const char* kInvalidPluginsPublic;
  extern const char* kInvalidRunAt;
  extern const char* kInvalidSidebar;
  extern const char* kInvalidSidebarDefaultIconPath;
  extern const char* kInvalidSidebarDefaultPage;
  extern const char* kInvalidSidebarDefaultTitle;
  extern const char* kInvalidSignature;
  extern const char* kInvalidTheme;
  extern const char* kInvalidThemeColors;
  extern const char* kInvalidThemeImages;
  extern const char* kInvalidThemeImagesMissing;
  extern const char* kInvalidThemeTints;
  extern const char* kInvalidToolstrip;
  extern const char* kInvalidToolstrips;
  extern const char* kInvalidTts;
  extern const char* kInvalidTtsVoices;
  extern const char* kInvalidTtsVoicesGender;
  extern const char* kInvalidTtsVoicesLocale;
  extern const char* kInvalidTtsVoicesVoiceName;
  extern const char* kInvalidUpdateURL;
  extern const char* kInvalidVersion;
  extern const char* kInvalidWebURL;
  extern const char* kInvalidWebURLs;
  extern const char* kInvalidZipHash;
  extern const char* kLaunchPathAndURLAreExclusive;
  extern const char* kLaunchURLRequired;
  extern const char* kLocalesMessagesFileMissing;
  extern const char* kLocalesNoDefaultLocaleSpecified;
  extern const char* kLocalesNoDefaultMessages;
  extern const char* kLocalesNoValidLocaleNamesListed;
  extern const char* kLocalesTreeMissing;
  extern const char* kManifestParseError;
  extern const char* kManifestUnreadable;
  extern const char* kMissingFile;
  extern const char* kMultipleOverrides;
  extern const char* kOneUISurfaceOnly;
  extern const char* kReservedMessageFound;
  extern const char* kSidebarExperimental;
  extern const char* kThemesCannotContainExtensions;
  extern const char* kWebContentMustBeEnabled;
#if defined(OS_CHROMEOS)
  extern const char* kIllegalPlugins;
#endif
}  // namespace extension_manifest_errors

namespace extension_urls {
  // The greatest common prefixes of the main extensions gallery's browse and
  // download URLs.
  extern const char* kGalleryBrowsePrefix;

  // Same thing for the "minigallery". The minigallery is the temporary static
  // themes gallery that we put up when we launched themes.
  extern const char* kMiniGalleryBrowsePrefix;
  extern const char* kMiniGalleryDownloadPrefix;
}  // namespace extension_urls

namespace extension_filenames {
  // The name of a temporary directory to install an extension into for
  // validation before finalizing install.
  extern const char* kTempExtensionName;

  // The file to write our decoded images to, relative to the extension_path.
  extern const char* kDecodedImagesFilename;

  // The file to write our decoded message catalogs to, relative to the
  // extension_path.
  extern const char* kDecodedMessageCatalogsFilename;
}

namespace extension_misc {
  const int kUnknownWindowId = -1;

  // The extension id of the bookmark manager.
  extern const char* kBookmarkManagerId;

  // The extension id of the Web Store component application.
  extern const char* kWebStoreAppId;

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
    LAUNCH_TAB
  };

  // The name of the apps promo histogram.
  extern const char* kAppsPromoHistogram;

  // The buckets used in the apps promo histogram.
  enum AppsPromoBuckets {
    PROMO_LAUNCH_APP,
    PROMO_LAUNCH_WEB_STORE,
    PROMO_CLOSE,
    PROMO_EXPIRE,
    PROMO_SEEN,
    PROMO_BUCKET_BOUNDARY
  };

  // The name of the app launch histogram.
  extern const char* kAppLaunchHistogram;

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

    // Launched from omnibox app links (not implemented yet).
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

    APP_LAUNCH_BUCKET_BOUNDARY,
    APP_LAUNCH_BUCKET_INVALID
  };

#if defined(OS_CHROMEOS)
  // The directory path on a ChromeOS device where accessibility extensions are
  // stored.
  extern const char* kAccessExtensionPath;
#endif
}  // extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
