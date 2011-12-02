// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media.h"
#include "base/logging.h"

namespace media {

bool InitializeMediaLibrary(const FilePath& module_dir) {
  // Android doesn't require any additional media libraries.
  return true;
}

void InitializeMediaLibraryForTesting() {}

bool IsMediaLibraryInitialized() {
  return true;
}

bool InitializeOpenMaxLibrary(const FilePath& module_dir) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace media
