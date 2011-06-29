// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEBUGGER_DEVTOOLS_FILE_UTIL_H_
#define CHROME_BROWSER_DEBUGGER_DEVTOOLS_FILE_UTIL_H_
#pragma once

#include <string>

#include "base/basictypes.h"

class Profile;

class DevToolsFileUtil {
 public:

  // Shows Save As dialog using default save location from the |profile| prefs,
  // |suggested_file_name| as the default name and saves |content| to the given
  // location.
  static void SaveAs(Profile* profile,
                     const std::string& suggested_file_name,
                     const std::string& content);

 private:
  DISALLOW_COPY_AND_ASSIGN(DevToolsFileUtil);
};

#endif  // CHROME_BROWSER_DEBUGGER_DEVTOOLS_FILE_UTIL_H_
