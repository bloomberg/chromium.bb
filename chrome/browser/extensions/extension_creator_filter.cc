// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_creator_filter.h"

#include "base/files/file_path.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace extensions {

bool ExtensionCreatorFilter::ShouldPackageFile(
    const base::FilePath& file_path) {
  const base::FilePath& base_name = file_path.BaseName();
  if (base_name.empty()) {
    return false;
  }

  base::FilePath::CharType first_character = base_name.value()[0];
  base::FilePath::CharType last_character =
      base_name.value()[base_name.value().length() - 1];

  // dotfile
  if (first_character == '.') {
    return false;
  }
  // Emacs backup file
  if (last_character == '~') {
    return false;
  }
  // Emacs auto-save file
  if (first_character == '#' && last_character == '#') {
    return false;
  }
  // Magic OS X file
  if (base_name.value() == FILE_PATH_LITERAL("__MACOSX")) {
    return false;
  }

#if defined(OS_WIN)
  // It's correct that we use file_path, not base_name, here, because we
  // are working with the actual file.
  DWORD file_attributes = ::GetFileAttributes(file_path.value().c_str());
  if (file_attributes == INVALID_FILE_ATTRIBUTES) {
    return false;
  }
  if ((file_attributes & FILE_ATTRIBUTE_HIDDEN) != 0) {
    return false;
  }
#endif

  return true;
}

}  // namespace extensions
