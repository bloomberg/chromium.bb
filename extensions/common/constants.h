// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_CONSTANTS_H_
#define EXTENSIONS_COMMON_CONSTANTS_H_

#include "base/files/file_path.h"

namespace extensions {

// Scheme we serve extension content from.
extern const char kExtensionScheme[];

  // The name of the manifest inside an extension.
extern const base::FilePath::CharType kManifestFilename[];

  // The name of locale folder inside an extension.
extern const base::FilePath::CharType kLocaleFolder[];

  // The name of the messages file inside an extension.
extern const base::FilePath::CharType kMessagesFilename[];

// The base directory for subdirectories with platform-specific code.
extern const base::FilePath::CharType kPlatformSpecificFolder[];

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_CONSTANTS_H_
