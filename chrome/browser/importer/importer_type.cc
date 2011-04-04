// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_type.h"

#include "base/logging.h"
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

Importer* CreateImporterByType(ImporterType type) {
  switch (type) {
#if defined(OS_WIN)
    case MS_IE:
      return new IEImporter();
#endif
    case BOOKMARKS_HTML:
    case FIREFOX2:
      return new Firefox2Importer();
    case FIREFOX3:
      return new Firefox3Importer();
    case GOOGLE_TOOLBAR5:
      return new Toolbar5Importer();
#if defined(OS_MACOSX)
    case SAFARI:
      return new SafariImporter(base::mac::GetUserLibraryPath());
#endif  // OS_MACOSX
    case NONE_IMPORTER:
      NOTREACHED();
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

}  // namespace importer
