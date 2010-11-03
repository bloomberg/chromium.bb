// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines dummy implementation of several functions from the
// master_preferences namespace for Google Chrome. These functions allow 64-bit
// Windows Chrome binary to build successfully. Since this binary is only used
// for Native Client support which uses the 32 bit installer, most of the
// master preferences functionality is not actually needed.

#include "chrome/installer/util/master_preferences.h"

#include <windows.h>

#include "base/logging.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"

namespace installer_util {

MasterPreferences::MasterPreferences(const CommandLine& cmd_line)
    : distribution_(NULL), preferences_read_from_file_(false) {
}

MasterPreferences::MasterPreferences(const FilePath& prefs_path)
    : distribution_(NULL), preferences_read_from_file_(false) {
}

MasterPreferences::~MasterPreferences() {
}

bool MasterPreferences::GetBool(const std::string& name, bool* value) const {
  // This function is called by InstallUtil::IsChromeFrameProcess()
  // We return false because GetInstallPreferences returns an empty value below.
  return false;
}

bool MasterPreferences::GetInt(const std::string& name, int* value) const {
  NOTREACHED();
  return false;
}

bool MasterPreferences::GetString(const std::string& name,
                                  std::string* value) const {
  NOTREACHED();
  return false;
}

std::vector<GURL> MasterPreferences::GetFirstRunTabs() const {
  NOTREACHED();
  return std::vector<GURL>();
}

}
