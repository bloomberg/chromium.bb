// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"

// Keys used in JSON representation of extensions.
namespace extension_manifest_keys {
  extern const char* kAllFrames;
  extern const char* kAltKey;
  extern const char* kApp;
  extern const char* kBackground;
  extern const char* kBrowserAction;
  extern const char* kBrowseURLs;
  extern const char* kChromeURLOverrides;
  extern const char* kContentScripts;
  extern const char* kContentSecurityPolicy;
  extern const char* kConvertedFromUserScript;
  extern const char* kCss;
  extern const char* kCtrlKey;
  extern const char* kCurrentLocale;
  extern const char* kDefaultLocale;
  extern const char* kDescription;
  extern const char* kDevToolsPage;
  extern const char* kExcludeGlobs;
  extern const char* kExcludeMatches;
  extern const char* kFileFilters;
  extern const char* kFileBrowserHandlers;
  extern const char* kHomepageURL;
  extern const char* kIcons;
  extern const char* kId;
  extern const char* kIncognito;
  extern const char* kIncludeGlobs;
  extern const char* kInputComponents;
  extern const char* kIntents;
  extern const char* kIntentType;
  extern const char* kIntentPath;
  extern const char* kIntentTitle;
  extern const char* kIntentDisposition;
  extern const char* kIsolation;
  extern const char* kJs;
  extern const char* kKeycode;
  extern const char* kLanguage;
  extern const char* kLaunch;
  extern const char* kLaunchContainer;
  extern const char* kLaunchHeight;
  extern const char* kLaunchLocalPath;
  extern const char* kLaunchWebURL;
  extern const char* kLaunchWidth;
  extern const char* kLayouts;
  extern const char* kManifestVersion;
  extern const char* kMatches;
  extern const char* kMinimumChromeVersion;
  extern const char* kNaClModules;
  extern const char* kNaClModulesMIMEType;
  extern const char* kNaClModulesPath;
  extern const char* kName;
  extern const char* kOfflineEnabled;
  extern const char* kOmnibox;
  extern const char* kOmniboxKeyword;
  extern const char* kOptionalPermissions;
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
  extern const char* kPlatformApp;
  extern const char* kPlugins;
  extern const char* kPluginsPath;
  extern const char* kPluginsPublic;
  extern const char* kPublicKey;
  extern const char* kRequirements;
  extern const char* kRunAt;
  extern const char* kShiftKey;
  extern const char* kShortcutKey;
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
  extern const char* kTtsEngine;
  extern const char* kTtsGenderFemale;
  extern const char* kTtsGenderMale;
  extern const char* kTtsVoices;
  extern const char* kTtsVoicesEventTypeEnd;
  extern const char* kTtsVoicesEventTypeError;
  extern const char* kTtsVoicesEventTypeMarker;
  extern const char* kTtsVoicesEventTypeSentence;
  extern const char* kTtsVoicesEventTypeStart;
  extern const char* kTtsVoicesEventTypeWord;
  extern const char* kTtsVoicesEventTypes;
  extern const char* kTtsVoicesGender;
  extern const char* kTtsVoicesLang;
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
  extern const char* kIntentDispositionWindow;
  extern const char* kIntentDispositionInline;
  extern const char* kIsolatedStorage;
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
  extern const char* kBackgroundPermissionNeeded;
  extern const char* kCannotAccessPage;
  extern const char* kCannotChangeExtensionID;
  extern const char* kCannotClaimAllHostsInExtent;
  extern const char* kCannotClaimAllURLsInExtent;
  extern const char* kCannotScriptGallery;
  extern const char* kCannotUninstallManagedExtension;
  extern const char* kChromeVersionTooLow;
  extern const char* kDevToolsExperimental;
  extern const char* kDisabledByPolicy;
  extern const char* kExperimentalFlagRequired;
  extern const char* kExpectString;
  extern const char* kHostedAppsCannotIncludeExtensionFeatures;
  extern const char* kInvalidAllFrames;
  extern const char* kInvalidBackground;
  extern const char* kInvalidBackgroundInHostedApp;
  extern const char* kInvalidBrowserAction;
  extern const char* kInvalidBrowseURL;
  extern const char* kInvalidBrowseURLs;
  extern const char* kInvalidChromeURLOverrides;
  extern const char* kInvalidContentScript;
  extern const char* kInvalidContentScriptsList;
  extern const char* kInvalidContentSecurityPolicy;
  extern const char* kInvalidCss;
  extern const char* kInvalidCssList;
  extern const char* kInvalidDefaultLocale;
  extern const char* kInvalidDescription;
  extern const char* kInvalidDevToolsPage;
  extern const char* kInvalidExcludeMatch;
  extern const char* kInvalidExcludeMatches;
  extern const char* kInvalidFileBrowserHandler;
  extern const char* kInvalidFileFiltersList;
  extern const char* kInvalidFileFilterValue;
  extern const char* kInvalidGlob;
  extern const char* kInvalidGlobList;
  extern const char* kInvalidHomepageURL;
  extern const char* kInvalidIconPath;
  extern const char* kInvalidIcons;
  extern const char* kInvalidIncognitoBehavior;
  extern const char* kInvalidInputComponents;
  extern const char* kInvalidInputComponentDescription;
  extern const char* kInvalidInputComponentLayoutName;
  extern const char* kInvalidInputComponentLayouts;
  extern const char* kInvalidInputComponentName;
  extern const char* kInvalidInputComponentShortcutKey;
  extern const char* kInvalidInputComponentShortcutKeycode;
  extern const char* kInvalidInputComponentType;
  extern const char* kInvalidIntent;
  extern const char* kInvalidIntentDisposition;
  extern const char* kInvalidIntentPath;
  extern const char* kInvalidIntents;
  extern const char* kInvalidIntentType;
  extern const char* kInvalidIntentTitle;
  extern const char* kInvalidIsolation;
  extern const char* kInvalidIsolationValue;
  extern const char* kInvalidJs;
  extern const char* kInvalidJsList;
  extern const char* kInvalidKey;
  extern const char* kInvalidLaunchContainer;
  extern const char* kInvalidLaunchContainerForPlatform;
  extern const char* kInvalidLaunchHeight;
  extern const char* kInvalidLaunchHeightContainer;
  extern const char* kInvalidLaunchLocalPath;
  extern const char* kInvalidLaunchWebURL;
  extern const char* kInvalidLaunchWidth;
  extern const char* kInvalidLaunchWidthContainer;
  extern const char* kInvalidManifest;
  extern const char* kInvalidManifestVersion;
  extern const char* kInvalidMatch;
  extern const char* kInvalidMatchCount;
  extern const char* kInvalidMatches;
  extern const char* kInvalidMinimumChromeVersion;
  extern const char* kInvalidNaClModules;
  extern const char* kInvalidNaClModulesMIMEType;
  extern const char* kInvalidNaClModulesPath;
  extern const char* kInvalidName;
  extern const char* kInvalidOfflineEnabled;
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
  extern const char* kInvalidRequirement;
  extern const char* kInvalidRequirements;
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
  extern const char* kInvalidTts;
  extern const char* kInvalidTtsVoices;
  extern const char* kInvalidTtsVoicesEventTypes;
  extern const char* kInvalidTtsVoicesGender;
  extern const char* kInvalidTtsVoicesLang;
  extern const char* kInvalidTtsVoicesVoiceName;
  extern const char* kInvalidUpdateURL;
  extern const char* kInvalidURLPatternError;
  extern const char* kInvalidVersion;
  extern const char* kInvalidWebURL;
  extern const char* kInvalidWebURLs;
  extern const char* kInvalidZipHash;
  extern const char* kLaunchPathAndExtentAreExclusive;
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
  extern const char* kNoWildCardsInPaths;
  extern const char* kPermissionNotAllowed;
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
  // Returns the URL prefix for the extension/apps gallery. Can be set via the
  // --apps-gallery-url switch. The URL returned will not contain a trailing
  // slash. Do not use this as a prefix/extent for the store.  Instead see
  // ExtensionService::GetWebStoreApp or
  // ExtensionService::IsDownloadFromGallery
  std::string GetWebstoreLaunchURL();

  // Returns the URL prefix for an item in the extension/app gallery. This URL
  // will contain a trailing slash and should be concatenated with an item ID
  // to get the item detail URL.
  std::string GetWebstoreItemDetailURLPrefix();

  // Returns the URL used to get webstore data (ratings, manifest, icon URL,
  // etc.) about an extension from the webstore as JSON.
  GURL GetWebstoreItemJsonDataURL(const std::string& extension_id);

  // Return the update URL used by gallery/webstore extensions/apps. The
  // |secure| parameter will be ignored if the update URL is overriden with
  // --apps-gallery-update-url.
  GURL GetWebstoreUpdateUrl(bool secure);

  // The greatest common prefixes of the main extensions gallery's browse and
  // download URLs.
  extern const char* kGalleryBrowsePrefix;
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

  // The extension id of the Cloud Print component application.
  extern const char* kCloudPrintAppId;

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

    APP_LAUNCH_BUCKET_BOUNDARY,
    APP_LAUNCH_BUCKET_INVALID
  };

#if defined(OS_CHROMEOS)
  // The directory path on a ChromeOS device where accessibility extensions are
  // stored.
  extern const char* kAccessExtensionPath;
  extern const char* kChromeVoxDirectoryName;
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
  };
}  // extension_misc

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
