// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_

#include <string>
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;

namespace content {
struct FaviconURL;
}

namespace android_webview {

// A helper that observes favicon changes for Webview.
class IconHelper : public content::WebContentsObserver {
 public:
  class Listener {
   public:
    virtual void OnReceivedIcon(const SkBitmap& bitmap) = 0;
    virtual void OnReceivedTouchIconUrl(const std::string& url,
                                        const bool precomposed) = 0;
   protected:
    virtual ~Listener() {}
  };

  explicit IconHelper(content::WebContents* web_contents);
  virtual ~IconHelper();

  void SetListener(Listener* listener);

  // From WebContentsObserver
  virtual void DidUpdateFaviconURL(int32 page_id,
      const std::vector<content::FaviconURL>& candidates) OVERRIDE;

  void DownloadFaviconCallback(int id, const GURL& image_url,
      int requested_size, const std::vector<SkBitmap>& bitmaps);

 private:
  Listener* listener_;

  DISALLOW_COPY_AND_ASSIGN(IconHelper);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_ICON_HELPER_H_
