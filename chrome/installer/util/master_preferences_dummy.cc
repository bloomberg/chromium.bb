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
#include "googleurl/src/gurl.h"

namespace installer_util {

bool GetDistroBooleanPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                bool* value) {
  NOTREACHED();
  return false;
}

bool GetDistroIntegerPreference(const DictionaryValue* prefs,
                                const std::wstring& name,
                                int* value) {
  NOTREACHED();
  return false;
}

DictionaryValue* GetInstallPreferences(const CommandLine& cmd_line) {
  NOTREACHED();
  return NULL;
}

DictionaryValue* ParseDistributionPreferences(
    const FilePath& master_prefs_path) {
  NOTREACHED();
  return NULL;
}

std::vector<GURL> GetFirstRunTabs(const DictionaryValue* prefs) {
  NOTREACHED();
  return std::vector<GURL>();
}

std::vector<GURL> GetDefaultBookmarks(const DictionaryValue* prefs) {
  NOTREACHED();
  return std::vector<GURL>();
}

bool SetDistroBooleanPreference(DictionaryValue* prefs,
                                const std::wstring& name,
                                bool value) {
  NOTREACHED();
  return false;
}

bool HasExtensionsBlock(const DictionaryValue* prefs,
                        DictionaryValue** extensions) {
  NOTREACHED();
  return false;
}

}
