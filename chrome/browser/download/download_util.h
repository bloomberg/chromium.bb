// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download utilities.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_

#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "ui/gfx/native_widget_types.h"

#if defined(TOOLKIT_VIEWS)
#include "ui/views/view.h"
#endif

class GURL;

namespace base {
class DictionaryValue;
}

namespace content {
class DownloadItem;
}

namespace gfx {
class Canvas;
class Image;
class ImageSkia;
class Rect;
}

namespace download_util {

// Download temporary file creation --------------------------------------------

// Return the default download directory.
const base::FilePath& GetDefaultDownloadDirectory();

// Return true if the |download_path| is dangerous path.
bool DownloadPathIsDangerous(const base::FilePath& download_path);

// Drag support ----------------------------------------------------------------

// Helper function for download views to use when acting as a drag source for a
// DownloadItem. If |icon| is NULL, no image will be accompany the drag. |view|
// is only required for Mac OS X, elsewhere it can be NULL.
void DragDownload(const content::DownloadItem* download,
                  gfx::Image* icon,
                  gfx::NativeView view);

// Helpers ---------------------------------------------------------------------

// Get the localized status text for an in-progress download.
string16 GetProgressStatusText(content::DownloadItem* download);

// Summer/Fall 2013 Finch experiment strings -----------------------------------
// Only deployed to English speakers, don't need translation.

// Study and condition names.
extern const char kFinchTrialName[];
extern const char kCondition1Control[];
extern const char kCondition2Control[];
extern const char kCondition3Malicious[];
extern const char kCondition4Unsafe[];
extern const char kCondition5Dangerous[];
extern const char kCondition6Harmful[];
extern const char kCondition7DiscardSecond[];
extern const char kCondition8DiscardFirst[];
extern const char kCondition9SafeDiscard[];
extern const char kCondition10SafeDontRun[];

// Helper for getting the appropriate message for a Finch trial.
// You should only invoke this if you believe you're in the kFinchTrialName
// finch trial; if you aren't, use the default string and don't invoke this.
base::string16 AssembleMalwareFinchString(const std::string& trial_condition,
                                          const string16& elided_filename);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
