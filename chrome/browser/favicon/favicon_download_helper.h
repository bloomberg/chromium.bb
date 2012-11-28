// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAVICON_FAVICON_DOWNLOAD_HELPER_H_
#define CHROME_BROWSER_FAVICON_FAVICON_DOWNLOAD_HELPER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/favicon_url.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

class GURL;
class FaviconDownloadHelperDelegate;
class SkBitmap;

// FaviconDownloadHelper handles requests to download favicons, and listens for
// the IPC messages from the renderer.
class FaviconDownloadHelper : public content::WebContentsObserver {
 public:
  FaviconDownloadHelper(content::WebContents* web_contents,
                        FaviconDownloadHelperDelegate* delegate);

  virtual ~FaviconDownloadHelper();

  // Starts the download of the given favicon |url| and returns the unique id of
  // the download request. When the download is finished, an
  // IconHostMsg_DidDownloadFavicon IPC message will be sent and passed on to
  // the delegate via FaviconDownloadHelperDelegate::OnDidDownloadFavicon().
  // Note that |image_size| is a hint for images with multiple sizes. The
  // downloaded image is not resized to the given image_size. If 0 is passed,
  // the first frame of the image is returned.
  int DownloadFavicon(const GURL& url, int image_size);

 protected:
  // content::WebContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  typedef std::set<int> DownloadIdList;

  // Message handler for IconHostMsg_DidDownloadFavicon. Called when the icon
  // at |image_url| has been downloaded.
  // |bitmaps| is a list of all the frames of the icon at |image_url|.
  void OnDidDownloadFavicon(int id,
                            const GURL& image_url,
                            bool errored,
                            int requested_size,
                            const std::vector<SkBitmap>& bitmaps);

  // Message Handler, called when the FaviconURL associated with the specified
  // page is updated.
  void OnUpdateFaviconURL(int32 page_id,
                          const std::vector<FaviconURL>& candidates);

  // Delegate to pass Favicon bitmaps back to. Weak.
  FaviconDownloadHelperDelegate* delegate_;

  // Ids of pending downloads.
  DownloadIdList download_ids_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FaviconDownloadHelper);
};

#endif  // CHROME_BROWSER_FAVICON_FAVICON_DOWNLOAD_HELPER_H_
