// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_

#include "ui/gfx/native_widget_types.h"

namespace base {
class FilePath;
}

namespace content {
class DownloadItem;
}

namespace gfx {
class Image;
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

}  // namespace download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_UTIL_H_
