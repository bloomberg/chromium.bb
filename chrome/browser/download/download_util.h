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

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
