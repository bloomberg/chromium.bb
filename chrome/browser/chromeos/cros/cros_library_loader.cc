// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library_loader.h"

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "cros/chromeos_cros_api.h"

namespace chromeos {

bool CrosLibraryLoader::Load(std::string* load_error_string) {
  bool loaded = false;
  FilePath path;
  if (PathService::Get(chrome::FILE_CHROMEOS_API, &path))
    loaded = LoadLibcros(path.value().c_str(), *load_error_string);

  if (!loaded) {
    LOG(ERROR) << "Problem loading chromeos shared object: "
               << *load_error_string;
  }
  return loaded;
}

}   // namespace chromeos

