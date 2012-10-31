// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_DOWNLOAD_HELPER_DELEGATE_H_
#define CHROME_BROWSER_FAVICON_FAVICON_DOWNLOAD_HELPER_DELEGATE_H_

#include <vector>

class GURL;
class SkBitmap;
struct FaviconURL;

// This class provides a delegate interface for a FaviconDownloadHelper.  It
// allows the FaviconDownloadHelper to pass favicon data back to its caller.
class FaviconDownloadHelperDelegate {
 public:
  // Called when the icon at |image_url| has been downloaded.
  // |bitmaps| is a list of all the frames of the icon at |image_url|.
  virtual void OnDidDownloadFavicon(
      int id,
      const GURL& image_url,
      bool errored,
      int requested_size,
      const std::vector<SkBitmap>& bitmaps) = 0;

  // Message Handler.
  virtual void OnUpdateFaviconURL(
      int32 page_id,
      const std::vector<FaviconURL>& candidates) = 0;

 protected:
  virtual ~FaviconDownloadHelperDelegate() {}
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_DOWNLOAD_HELPER_DELEGATE_H_
