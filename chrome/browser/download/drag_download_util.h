// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
#define CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_

#include "base/basictypes.h"
#include "base/file_path.h"

class GURL;

namespace drag_download_util {

// Parse the download metadata set in DataTransfer.setData. The metadata
// consists of a set of the following values separated by ":"
// * MIME type
// * File name
// * URL
// For example, we can have
//   text/plain:example.txt:http://example.com/example.txt
bool ParseDownloadMetadata(const string16& metadata,
                           string16* mime_type,
                           FilePath* file_name,
                           GURL* url);

}  // namespace drag_download_util

#endif  // CHROME_BROWSER_DOWNLOAD_DRAG_DOWNLOAD_UTIL_H_
