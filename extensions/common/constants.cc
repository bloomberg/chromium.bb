// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/constants.h"

#include "base/files/file_path.h"

namespace extensions {

const char kExtensionScheme[] = "chrome-extension";
const char kExtensionResourceScheme[] = "chrome-extension-resource";

const base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");
const base::FilePath::CharType kLocaleFolder[] =
    FILE_PATH_LITERAL("_locales");
const base::FilePath::CharType kMessagesFilename[] =
    FILE_PATH_LITERAL("messages.json");
const base::FilePath::CharType kPlatformSpecificFolder[] =
    FILE_PATH_LITERAL("_platform_specific");
const base::FilePath::CharType kMetadataFolder[] =
    FILE_PATH_LITERAL("_metadata");
const base::FilePath::CharType kVerifiedContentsFilename[] =
    FILE_PATH_LITERAL("verified_contents.json");
const base::FilePath::CharType kComputedHashesFilename[] =
    FILE_PATH_LITERAL("computed_hashes.json");

const char kInstallDirectoryName[] = "Extensions";

const char kTempExtensionName[] = "CRX_INSTALL";

const char kDecodedImagesFilename[] = "DECODED_IMAGES";

const char kDecodedMessageCatalogsFilename[] = "DECODED_MESSAGE_CATALOGS";

const char kGeneratedBackgroundPageFilename[] =
    "_generated_background_page.html";

const char kModulesDir[] = "_modules";

const base::FilePath::CharType kExtensionFileExtension[] =
    FILE_PATH_LITERAL(".crx");
const base::FilePath::CharType kExtensionKeyFileExtension[] =
    FILE_PATH_LITERAL(".pem");

// If auto-updates are turned on, default to running every 5 hours.
const int kDefaultUpdateFrequencySeconds = 60 * 60 * 5;

const char kLocalAppSettingsDirectoryName[] = "Local App Settings";
const char kLocalExtensionSettingsDirectoryName[] = "Local Extension Settings";
const char kSyncAppSettingsDirectoryName[] = "Sync App Settings";
const char kSyncExtensionSettingsDirectoryName[] = "Sync Extension Settings";
const char kManagedSettingsDirectoryName[] = "Managed Extension Settings";
const char kStateStoreName[] = "Extension State";
const char kRulesStoreName[] = "Extension Rules";

}  // namespace extensions

namespace extension_misc {

const int kExtensionIconSizes[] = {EXTENSION_ICON_GIGANTOR,     // 512
                                   EXTENSION_ICON_EXTRA_LARGE,  // 256
                                   EXTENSION_ICON_LARGE,        // 128
                                   EXTENSION_ICON_MEDIUM,       // 48
                                   EXTENSION_ICON_SMALL,        // 32
                                   EXTENSION_ICON_SMALLISH,     // 24
                                   EXTENSION_ICON_BITTY,        // 16
                                   // Additional 2x resources to load.
                                   2 * EXTENSION_ICON_MEDIUM,  // 96
                                   2 * EXTENSION_ICON_SMALL    // 64
};

const size_t kNumExtensionIconSizes = arraysize(kExtensionIconSizes);

const int kExtensionActionIconSizes[] = {EXTENSION_ICON_ACTION,     // 19,
                                         2 * EXTENSION_ICON_ACTION  // 38
};

const size_t kNumExtensionActionIconSizes =
    arraysize(kExtensionActionIconSizes);

}  // namespace extension_misc
