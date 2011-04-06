// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_DATA_TYPES_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_DATA_TYPES_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/string16.h"
#include "chrome/browser/importer/importer_type.h"

// Types needed for importing data from other browsers and the Google Toolbar.
namespace importer {

// An enumeration of the type of data that can be imported.
enum ImportItem {
  NONE           = 0,
  HISTORY        = 1 << 0,
  FAVORITES      = 1 << 1,
  COOKIES        = 1 << 2,  // Not supported yet.
  PASSWORDS      = 1 << 3,
  SEARCH_ENGINES = 1 << 4,
  HOME_PAGE      = 1 << 5,
  ALL            = (1 << 6) - 1  // All the bits should be 1, hence the -1.
};

// Information about a profile needed by an importer to do import work.
struct SourceProfile {
  SourceProfile();
  ~SourceProfile();

  string16 importer_name;
  ImporterType importer_type;
  FilePath source_path;
  FilePath app_path;
  uint16 services_supported;  // Bitmask of ImportItem.
};

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_DATA_TYPES_H_
