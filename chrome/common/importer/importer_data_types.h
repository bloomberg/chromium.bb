// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTER_DATA_TYPES_H_
#define CHROME_COMMON_IMPORTER_IMPORTER_DATA_TYPES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/common/importer/importer_type.h"
#include "url/gurl.h"

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

  base::string16 importer_name;
  ImporterType importer_type;
  base::FilePath source_path;
  base::FilePath app_path;
  uint16 services_supported;  // Bitmask of ImportItem.
  // The application locale. Stored because we can only access it from the UI
  // thread on the browser process. This is only used by the Firefox importer.
  std::string locale;
};

// Contains information needed for importing bookmarks/search engine urls, etc.
struct URLKeywordInfo {
  GURL url;
  base::string16 keyword;
  base::string16 display_name;
};

#if defined(OS_WIN)
// Contains the information read from the IE7/IE8 Storage2 key in the registry.
struct ImporterIE7PasswordInfo {
  // Hash of the url.
  std::wstring url_hash;

  // Encrypted data containing the username, password and some more
  // undocumented fields.
  std::vector<unsigned char> encrypted_data;

  // When the login was imported.
  base::Time date_created;
};
#endif

// Mapped to history::VisitSource after import in the browser.
enum VisitSource {
  VISIT_SOURCE_BROWSED = 0,
  VISIT_SOURCE_FIREFOX_IMPORTED = 1,
  VISIT_SOURCE_IE_IMPORTED = 2,
  VISIT_SOURCE_SAFARI_IMPORTED = 3,
};

}  // namespace importer

#endif  // CHROME_COMMON_IMPORTER_IMPORTER_DATA_TYPES_H_
