// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/cros_library.h"

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
bool CrosLibrary::load_error_ = false;

// static
std::string CrosLibrary::load_error_string_;

// static
bool CrosLibrary::EnsureLoaded() {
  if (!loaded_ && !load_error_) {
    FilePath path;
    if (PathService::Get(chrome::FILE_CHROMEOS_API, &path))
      loaded_ = LoadLibcros(path.value().c_str(), load_error_string_);

    if (!loaded_) {
      load_error_ = true;
      LOG(ERROR) << "Problem loading chromeos shared object: "
                 << load_error_string_;
    }
  }
  return loaded_;
}

}  // namespace chromeos
