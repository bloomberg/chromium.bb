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

// Record the total number of items and the number of in-progress items showing
// in the shelf when it closes.  Set |autoclose| to true when the shelf is
// closing itself, false when the user explicitly closed it.
void RecordShelfClose(int size, int in_progress, bool autoclose);

// Used for counting UMA stats. Similar to content's
// download_stats::DownloadCountTypes but from the chrome layer.
enum ChromeDownloadCountTypes {
  // Stale enum values left around os that values passed to UMA don't
  // change.
  CHROME_DOWNLOAD_COUNT_UNUSED_0 = 0,
  CHROME_DOWNLOAD_COUNT_UNUSED_1,
  CHROME_DOWNLOAD_COUNT_UNUSED_2,
  CHROME_DOWNLOAD_COUNT_UNUSED_3,

  // A download *would* have been initiated, but it was blocked
  // by the DownloadThrottlingResourceHandler.
  BLOCKED_BY_THROTTLING,

  CHROME_DOWNLOAD_COUNT_TYPES_LAST_ENTRY
};

// Used for counting UMA stats. Similar to content's
// download_stats::DownloadInitiattionSources but from the chrome layer.
enum ChromeDownloadSource {
  // The download was initiated by navigating to a URL (e.g. by user click).
  INITIATED_BY_NAVIGATION = 0,

  // The download was initiated by invoking a context menu within a page.
  INITIATED_BY_CONTEXT_MENU,

  // The download was initiated by the WebStore installer.
  INITIATED_BY_WEBSTORE_INSTALLER,

  // The download was initiated by the ImageBurner (cros).
  INITIATED_BY_IMAGE_BURNER,

  // The download was initiated by the plugin installer.
  INITIATED_BY_PLUGIN_INSTALLER,

  // The download was initiated by the PDF plugin..
  INITIATED_BY_PDF_SAVE,

  CHROME_DOWNLOAD_SOURCE_LAST_ENTRY,
};

// Increment one of the above counts.
void RecordDownloadCount(ChromeDownloadCountTypes type);

// Record initiation of a download from a specific source.
void RecordDownloadSource(ChromeDownloadSource source);

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
