// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_TYPE_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_TYPE_H_

#include <string>

#include "build/build_config.h"

class Importer;

namespace importer {

// An enumeration of the type of importers that we support to import
// settings and data from (browsers, google toolbar and a bookmarks html file).
// NOTE: Numbers added so that data can be reliably cast to ints and passed
// across IPC.
enum ImporterType {
  TYPE_UNKNOWN         = -1,
#if defined(OS_WIN)
  TYPE_IE              = 0,
#endif
  TYPE_FIREFOX2        = 1,
  TYPE_FIREFOX3        = 2,  // Firefox 3 and later.
#if defined(OS_MACOSX)
  TYPE_SAFARI          = 3,
#endif
  TYPE_GOOGLE_TOOLBAR5 = 4,
  // Identifies a 'bookmarks.html' file.
  TYPE_BOOKMARKS_FILE  = 5
};

// Creates an Importer of the specified |type|.
Importer* CreateImporterByType(ImporterType type);

// Logs to UMA that an Importer of the specified |type| was used. Uses
// |metric_postfix| to split by entry point. Note: Values passed via
// |metric_postfix| require a matching "Import.ImporterType.|metric_postfix|"
// entry in tools/metrics/histograms/histograms.xml.
void LogImporterUseToMetrics(const std::string& metric_prefix,
                             ImporterType type);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_TYPE_H_
