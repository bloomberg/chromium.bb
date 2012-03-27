// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

namespace installer {

MasterPreferences::MasterPreferences(const CommandLine& cmd_line)
    : distribution_(NULL), preferences_read_from_file_(false) {
}

MasterPreferences::MasterPreferences(const FilePath& prefs_path)
    : distribution_(NULL), preferences_read_from_file_(false) {
}

MasterPreferences::~MasterPreferences() {
}

bool MasterPreferences::GetBool(const std::string& name, bool* value) const {
  NOTREACHED();
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

// static
const MasterPreferences& MasterPreferences::ForCurrentProcess() {
  static MasterPreferences prefs(*CommandLine::ForCurrentProcess());
  return prefs;
}
}

// The use of std::vector<GURL>() above requires us to have destructors for
// GURL and its contained types.  GURL contains a member of type
// url_parse::Parsed.  Both Parsed and GURL declare (but do not implement)
// explicit destructors in their header files.  We're missing the real
// implementations by not depending on the googleurl library.  However, we don't
// really need them, so we just replace them here rather than building a 64-bit
// version of the googleurl library with all its dependencies.

GURL::~GURL() {
  NOTREACHED();
}

namespace url_parse {

Parsed::~Parsed() {
  NOTREACHED();
}

}
