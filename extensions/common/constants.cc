// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/constants.h"

namespace extensions {

const char kExtensionScheme[] = "chrome-extension";

const base::FilePath::CharType kManifestFilename[] =
    FILE_PATH_LITERAL("manifest.json");
const base::FilePath::CharType kLocaleFolder[] =
    FILE_PATH_LITERAL("_locales");
const base::FilePath::CharType kMessagesFilename[] =
    FILE_PATH_LITERAL("messages.json");
const base::FilePath::CharType kPlatformSpecificFolder[] =
    FILE_PATH_LITERAL("_platform_specific");

} // namespace extensions
