// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/syslogs_library.h"

#include "base/string_util.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/cros_library.h"

namespace chromeos {

class SyslogsLibraryImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryImpl() {}
  virtual ~SyslogsLibraryImpl() {}

  LogDictionaryType* GetSyslogs(FilePath* tmpfilename) {
    if (CrosLibrary::Get()->EnsureLoaded()) {
      return chromeos::GetSystemLogs(tmpfilename);
    }
    return NULL;
  }
};

class SyslogsLibraryStubImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryStubImpl() {}
  virtual ~SyslogsLibraryStubImpl() {}

  LogDictionaryType* GetSyslogs(FilePath* tmpfilename) {
    return &log_dictionary_;
  }

 private:
  LogDictionaryType log_dictionary_;
};

// static
SyslogsLibrary* SyslogsLibrary::GetImpl(bool stub) {
  if (stub)
    return new SyslogsLibraryStubImpl();
  else
    return new SyslogsLibraryImpl();
}

}  // namespace chromeos
