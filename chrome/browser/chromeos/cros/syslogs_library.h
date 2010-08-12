// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
#pragma once

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

  // Factory function, creates a new instance and returns ownership.
  // For normal usage, access the singleton via CrosLibrary::Get().
  static SyslogsLibrary* GetImpl(bool stub);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_SYSLOGS_LIBRARY_H_
