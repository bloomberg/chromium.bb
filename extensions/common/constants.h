// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONSTANTS_H_
#define EXTENSIONS_COMMON_CONSTANTS_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "ui/base/layout.h"

namespace extensions {

// Scheme we serve extension content from.
extern const char kExtensionScheme[];

// Canonical schemes you can use as input to GURL.SchemeIs().
extern const char kExtensionResourceScheme[];

// The name of the manifest inside an extension.
extern const base::FilePath::CharType kManifestFilename[];

// The name of locale folder inside an extension.
extern const base::FilePath::CharType kLocaleFolder[];

// The name of the messages file inside an extension.
extern const base::FilePath::CharType kMessagesFilename[];

// The base directory for subdirectories with platform-specific code.
extern const base::FilePath::CharType kPlatformSpecificFolder[];

// A directory reserved for metadata, generated either by the webstore
// or chrome.
extern const base::FilePath::CharType kMetadataFolder[];

// Name of the verified contents file within the metadata folder.
extern const base::FilePath::CharType kVerifiedContentsFilename[];

// Name of the computed hashes file within the metadata folder.
extern const base::FilePath::CharType kComputedHashesFilename[];

// The name of the directory inside the profile where extensions are
// installed to.
extern const char kInstallDirectoryName[];

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

// Path to imported modules.
extern const char kModulesDir[];

// The file extension (.crx) for extensions.
extern const base::FilePath::CharType kExtensionFileExtension[];

// The file extension (.pem) for private key files.
extern const base::FilePath::CharType kExtensionKeyFileExtension[];

// Default frequency for auto updates, if turned on.
extern const int kDefaultUpdateFrequencySeconds;

// The name of the directory inside the profile where per-app local settings
// are stored.
extern const char kLocalAppSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension local
// settings are stored.
extern const char kLocalExtensionSettingsDirectoryName[];

// The name of the directory inside the profile where per-app synced settings
// are stored.
extern const char kSyncAppSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension synced
// settings are stored.
extern const char kSyncExtensionSettingsDirectoryName[];

// The name of the directory inside the profile where per-extension persistent
// managed settings are stored.
extern const char kManagedSettingsDirectoryName[];

// The name of the database inside the profile where chrome-internal
// extension state resides.
extern const char kStateStoreName[];

// The name of the database inside the profile where declarative extension
// rules are stored.
extern const char kRulesStoreName[];

// The URL query parameter key corresponding to multi-login user index.
extern const char kAuthUserQueryKey[];

// Mime type strings
extern const char kMimeTypeJpeg[];
extern const char kMimeTypePng[];

// The extension id of the Web Store component application.
extern const char kWebStoreAppId[];

}  // namespace extensions

namespace extension_misc {

// Matches chrome.windows.WINDOW_ID_NONE.
const int kUnknownWindowId = -1;

// Matches chrome.windows.WINDOW_ID_CURRENT.
const int kCurrentWindowId = -2;

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

struct IconRepresentationInfo {
  // Size in pixels.
  const int size;
  // Size as a string that will be used to retrieve representation value from
  // ExtensionAction SetIcon function arguments.
  const char* const size_string;
  // Scale factor for which the representation should be used.
  const ui::ScaleFactor scale;
};

// The icon representations for extension actions.
extern const IconRepresentationInfo kExtensionActionIconSizes[];
const size_t kNumExtensionActionIconSizes = 2u;

}  // namespace extension_misc

#endif  // EXTENSIONS_COMMON_CONSTANTS_H_
