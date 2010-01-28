// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_

// Keys used in JSON representation of extensions.
namespace extension_manifest_keys {
  extern const wchar_t* kAllFrames;
  extern const wchar_t* kApp;
  extern const wchar_t* kAppOrigins;
  extern const wchar_t* kAppLaunchUrl;
  extern const wchar_t* kBackground;
  extern const wchar_t* kBrowserAction;
  extern const wchar_t* kMinimumChromeVersion;
  extern const wchar_t* kChromeURLOverrides;
  extern const wchar_t* kContentScripts;
  extern const wchar_t* kConvertedFromUserScript;
  extern const wchar_t* kCss;
  extern const wchar_t* kCurrentLocale;
  extern const wchar_t* kDefaultLocale;
  extern const wchar_t* kDescription;
  extern const wchar_t* kExcludeGlobs;
  extern const wchar_t* kIcons;
  extern const wchar_t* kIncludeGlobs;
  extern const wchar_t* kJs;
  extern const wchar_t* kMatches;
  extern const wchar_t* kName;
  extern const wchar_t* kPageActionId;
  extern const wchar_t* kPageAction;
  extern const wchar_t* kPageActions;
  extern const wchar_t* kPageActionIcons;
  extern const wchar_t* kPageActionDefaultIcon;
  extern const wchar_t* kPageActionDefaultPopup;
  extern const wchar_t* kPageActionDefaultTitle;
  extern const wchar_t* kPageActionPopup;
  extern const wchar_t* kPageActionPopupHeight;
  extern const wchar_t* kPageActionPopupPath;
  extern const wchar_t* kPermissions;
  extern const wchar_t* kPlugins;
  extern const wchar_t* kPluginsPath;
  extern const wchar_t* kPluginsPublic;
  extern const wchar_t* kPublicKey;
  extern const wchar_t* kSignature;
  extern const wchar_t* kRunAt;
  extern const wchar_t* kTheme;
  extern const wchar_t* kThemeImages;
  extern const wchar_t* kThemeColors;
  extern const wchar_t* kThemeTints;
  extern const wchar_t* kThemeDisplayProperties;
  extern const wchar_t* kToolstripMoleHeight;
  extern const wchar_t* kToolstripMolePath;
  extern const wchar_t* kToolstripPath;
  extern const wchar_t* kToolstrips;
  extern const wchar_t* kType;
  extern const wchar_t* kVersion;
  extern const wchar_t* kUpdateURL;
  extern const wchar_t* kOptionsPage;
}  // namespace extension_manifest_keys

// Some values expected in manifests.
namespace extension_manifest_values {
  extern const char* kRunAtDocumentStart;
  extern const char* kRunAtDocumentEnd;
  extern const char* kRunAtDocumentIdle;
  extern const char* kPageActionTypeTab;
  extern const char* kPageActionTypePermanent;
}  // namespace extension_manifest_values

// Error messages returned from Extension::InitFromValue().
namespace extension_manifest_errors {
  extern const char* kChromeVersionTooLow;
  extern const char* kInvalidAllFrames;
  extern const char* kInvalidApp;
  extern const char* kInvalidAppOrigin;
  extern const char* kInvalidAppLaunchUrl;
  extern const char* kInvalidBackground;
  extern const char* kInvalidBrowserAction;
  extern const char* kInvalidChromeURLOverrides;
  extern const char* kInvalidContentScript;
  extern const char* kInvalidContentScriptsList;
  extern const char* kInvalidCss;
  extern const char* kInvalidCssList;
  extern const char* kInvalidDescription;
  extern const char* kInvalidIcons;
  extern const char* kInvalidIconPath;
  extern const char* kInvalidGlobList;
  extern const char* kInvalidGlob;
  extern const char* kInvalidJs;
  extern const char* kInvalidJsList;
  extern const char* kInvalidKey;
  extern const char* kInvalidManifest;
  extern const char* kInvalidMatchCount;
  extern const char* kInvalidMatch;
  extern const char* kInvalidMatches;
  extern const char* kInvalidMinimumChromeVersion;
  extern const char* kInvalidName;
  extern const char* kInvalidPlugins;
  extern const char* kInvalidPluginsPath;
  extern const char* kInvalidPluginsPublic;

  extern const char* kInvalidRunAt;
  extern const char* kInvalidSignature;
  extern const char* kInvalidToolstrip;
  extern const char* kInvalidToolstrips;
  extern const char* kInvalidVersion;
  extern const char* kInvalidPageAction;
  extern const char* kInvalidPageActionName;
  extern const char* kInvalidPageActionsList;
  extern const char* kInvalidPageActionsListSize;
  extern const char* kInvalidPageActionIconPath;
  extern const char* kInvalidPageActionId;
  extern const char* kInvalidPageActionDefaultTitle;
  extern const char* kInvalidPageActionOldAndNewKeys;
  extern const char* kInvalidPageActionPopup;
  extern const char* kInvalidPageActionPopupHeight;
  extern const char* kInvalidPageActionPopupPath;
  extern const char* kInvalidPageActionTypeValue;
  extern const char* kInvalidPermissions;
  extern const char* kInvalidPermission;
  extern const char* kInvalidPermissionScheme;
  extern const char* kInvalidZipHash;
  extern const char* kInvalidTheme;
  extern const char* kInvalidThemeImagesMissing;
  extern const char* kInvalidThemeImages;
  extern const char* kInvalidThemeColors;
  extern const char* kInvalidThemeTints;
  extern const char* kOneUISurfaceOnly;
  extern const char* kThemesCannotContainExtensions;
  extern const char* kManifestParseError;
  extern const char* kManifestUnreadable;
  extern const char* kMissingFile;
  extern const char* kInvalidUpdateURL;
  extern const char* kInvalidDefaultLocale;
  extern const char* kLocalesNoDefaultLocaleSpecified;
  extern const char* kLocalesNoDefaultMessages;
  extern const char* kLocalesNoValidLocaleNamesListed;
  extern const char* kLocalesTreeMissing;
  extern const char* kLocalesMessagesFileMissing;
  extern const char* kInvalidOptionsPage;
}  // namespace extension_manifest_errors

namespace extension_urls {
  // The greatest common prefixes of the main extensions gallery's browse and
  // download URLs.
  extern const char* kGalleryBrowsePrefix;
  extern const char* kGalleryDownloadPrefix;

  // Same thing for the "minigallery". The minigallery is the temporary static
  // themes gallery that we put up when we launched themes.
  extern const char* kMiniGalleryBrowsePrefix;
  extern const char* kMiniGalleryDownloadPrefix;
}  // namespace extension_urls

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_CONSTANTS_H_
