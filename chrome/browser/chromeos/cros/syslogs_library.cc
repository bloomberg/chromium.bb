// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/syslogs_library.h"

#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

LogDictionaryType* SyslogsLibraryImpl::GetSyslogs(FilePath* tmpfilename) {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    return chromeos::GetSystemLogs(tmpfilename);
  }
  return NULL;
}

}  // namespace chromeos
