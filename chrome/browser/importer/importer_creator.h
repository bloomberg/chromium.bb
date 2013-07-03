// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_CREATOR_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_CREATOR_H_

#include <string>

#include "chrome/common/importer/importer_type.h"

class Importer;

namespace importer {

// Creates an Importer of the specified |type|.
Importer* CreateImporterByType(ImporterType type);

// Logs to UMA that an Importer of the specified |type| was used. Uses
// |metric_postfix| to split by entry point. Note: Values passed via
// |metric_postfix| require a matching "Import.ImporterType.|metric_postfix|"
// entry in tools/metrics/histograms/histograms.xml.
void LogImporterUseToMetrics(const std::string& metric_prefix,
                             ImporterType type);

}  // namespace importer

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_CREATOR_H_
