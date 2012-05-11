// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/find_entry_callback.h"

namespace gdata {

void ReadOnlyFindEntryCallback(GDataEntry** out,
                               base::PlatformFileError error,
                               const FilePath& directory_path,
                               GDataEntry* entry) {
  if (error == base::PLATFORM_FILE_OK)
    *out = entry;
}

}  // namespace gdata
