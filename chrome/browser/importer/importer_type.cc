// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_type.h"

#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox3_importer.h"
#include "chrome/browser/importer/toolbar_importer.h"

#if defined(OS_WIN)
#include "chrome/browser/importer/ie_importer.h"
#endif

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "base/mac/foundation_util.h"
#include "chrome/browser/importer/safari_importer.h"
#endif

namespace importer {

namespace {

// The enum used to register importer use.
enum ImporterTypeMetrics {
  IMPORTER_METRICS_UNKNOWN         = 0,
#if defined(OS_WIN)
  IMPORTER_METRICS_IE              = 1,
#endif
  IMPORTER_METRICS_FIREFOX2        = 2,
  IMPORTER_METRICS_FIREFOX3        = 3,
#if defined(OS_MACOSX)
  IMPORTER_METRICS_SAFARI          = 4,
#endif
  IMPORTER_METRICS_GOOGLE_TOOLBAR5 = 5,
  IMPORTER_METRICS_BOOKMARKS_FILE  = 6,

  // Insert new values here. Never remove any existing values, as this enum is
  // used to bucket a UMA histogram, and removing values breaks that.
  IMPORTER_METRICS_SIZE
};


}  // namespace

Importer* CreateImporterByType(ImporterType type) {
  switch (type) {
#if defined(OS_WIN)
    case TYPE_IE:
      return new IEImporter();
#endif
    case TYPE_BOOKMARKS_FILE:
    case TYPE_FIREFOX2:
      return new Firefox2Importer();
    case TYPE_FIREFOX3:
      return new Firefox3Importer();
#if defined(OS_MACOSX)
    case TYPE_SAFARI:
      return new SafariImporter(base::mac::GetUserLibraryPath());
#endif
    case TYPE_GOOGLE_TOOLBAR5:
      return new Toolbar5Importer();
    default:
      NOTREACHED();
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

void LogImporterUseToMetrics(const std::string& metric_postfix,
                             ImporterType type) {
  ImporterTypeMetrics metrics_type = IMPORTER_METRICS_UNKNOWN;
  switch (type) {
    case TYPE_UNKNOWN:
      metrics_type = IMPORTER_METRICS_UNKNOWN;
      break;
#if defined(OS_WIN)
    case TYPE_IE:
      metrics_type = IMPORTER_METRICS_IE;
      break;
#endif
    case TYPE_FIREFOX2:
      metrics_type = IMPORTER_METRICS_FIREFOX2;
      break;
    case TYPE_FIREFOX3:
      metrics_type = IMPORTER_METRICS_FIREFOX3;
      break;
#if defined(OS_MACOSX)
    case TYPE_SAFARI:
      metrics_type = IMPORTER_METRICS_SAFARI;
      break;
#endif
    case TYPE_GOOGLE_TOOLBAR5:
      metrics_type = IMPORTER_METRICS_GOOGLE_TOOLBAR5;
      break;
    case TYPE_BOOKMARKS_FILE:
      metrics_type = IMPORTER_METRICS_BOOKMARKS_FILE;
      break;
  }

  // Note: This leaks memory, which is the expected behavior as the factory
  // creates and owns the histogram.
  base::HistogramBase* histogram =
      base::LinearHistogram::FactoryGet(
          "Import.ImporterType." + metric_postfix,
          1,
          IMPORTER_METRICS_SIZE,
          IMPORTER_METRICS_SIZE + 1,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(metrics_type);
}

}  // namespace importer
