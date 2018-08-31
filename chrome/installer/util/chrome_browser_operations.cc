// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/chrome_browser_operations.h"

#include "base/logging.h"
#include "chrome/installer/util/util_constants.h"

namespace installer {

void ChromeBrowserOperations::AddKeyFiles(
    std::vector<base::FilePath>* key_files) const {
  DCHECK(key_files);
  key_files->push_back(base::FilePath(kChromeDll));
}

}  // namespace installer
