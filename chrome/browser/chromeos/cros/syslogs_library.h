// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_

#include "base/singleton.h"
#include "cros/chromeos_syslogs.h"

namespace chromeos {

// This interface defines interaction with the ChromeOS syslogs APIs.
class SyslogsLibrary {
 public:
  SyslogsLibrary() {}
  virtual ~SyslogsLibrary() {}

  // System logs gathered for userfeedback
  virtual LogDictionaryType* GetSyslogs(FilePath* tmpfilename)  = 0;
};


// This class handles the interaction with the ChromeOS syslogs APIs.
class SyslogsLibraryImpl : public SyslogsLibrary {
 public:
  SyslogsLibraryImpl() {}
  virtual ~SyslogsLibraryImpl() {}

  virtual LogDictionaryType* GetSyslogs(FilePath* tmpfilename);

 private:
  DISALLOW_COPY_AND_ASSIGN(SyslogsLibraryImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
