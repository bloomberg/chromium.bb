// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_
#define CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_

#include "ui/base/layout.h"

class GURL;

namespace content {
class RenderViewHost;
}

namespace gfx {
class Image;
}

namespace history {
struct FaviconBitmapResult;
}

// Utility class for common favicon related code.
class FaviconUtil {
 public:
  // Starts the download of the given favicon |url| from the given render view
  // host. When the download is finished, an IconHostMsg_DidDownloadFavicon IPC
  // message will be sent.
  // Returns the unique id of the download request. The id will be sent in the
  // IPC message.
  static int DownloadFavicon(content::RenderViewHost* rvh,
                             const GURL& url,
                             int image_size);

  // Takes a vector of png-encoded frames, decodes them, and converts them to
  // a favicon of size favicon_size (in DIPs) at the desired ui scale factors.
  static gfx::Image SelectFaviconFramesFromPNGs(
      const std::vector<history::FaviconBitmapResult>& png_data,
      const std::vector<ui::ScaleFactor> scale_factors,
      int favicon_size);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_UTIL_H_
