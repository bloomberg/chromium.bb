// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_DATA_TYPES_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_DATA_TYPES_H_

#include <string>

#include "base/basictypes.h"

// Types needed for importing data from other browsers and the Google
// Toolbar.
namespace importer {

// An enumeration of the type of data that can be imported.
enum ImportItem {
  NONE           = 0x0000,
  HISTORY        = 0x0001,
  FAVORITES      = 0x0002,
  COOKIES        = 0x0004,  // Not supported yet.
  PASSWORDS      = 0x0008,
  SEARCH_ENGINES = 0x0010,
  HOME_PAGE      = 0x0020,
  ALL            = 0x003f
};

// An enumeration of the type of browsers that we support to import
// settings and data from them.  Numbers added so that data can be
// reliably cast to ints and passed across IPC.
enum ProfileType {
#if defined(OS_WIN)
  MS_IE = 0,
#endif
  FIREFOX2 = 1,
  FIREFOX3 = 2,
#if defined(OS_MACOSX)
  SAFARI = 3,
#endif
  GOOGLE_TOOLBAR5 = 4,
  // Identifies a 'bookmarks.html' file.
  BOOKMARKS_HTML = 5
};

// Information about a profile needed by an importer to do import work.
struct ProfileInfo {
  std::wstring description;
  importer::ProfileType browser_type;
  std::wstring source_path;
  std::wstring app_path;
  uint16 services_supported;  // Bitmap of ImportItem
};

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_DATA_TYPES_H_

