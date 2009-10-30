// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros_library.h"

#include <dlfcn.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "third_party/cros/chromeos_cros_api.h"

namespace chromeos {

// static
bool CrosLibrary::loaded_ = false;

// static
bool CrosLibrary::loaded() {
  static bool initialized = false;
  if (!initialized) {
    FilePath path;
    if (PathService::Get(chrome::FILE_CHROMEOS_API, &path))
      loaded_ = LoadCros(path.value().c_str());

    if (!loaded_) {
      char* error = dlerror();
      if (error)
        LOG(ERROR) << "Problem loading chromeos shared object: " << error;
    }
    initialized = true;
  }
  return loaded_;
}

}  // namespace chromeos
