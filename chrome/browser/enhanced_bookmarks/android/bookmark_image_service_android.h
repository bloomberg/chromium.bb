// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_BOOKMARK_IMAGE_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_BOOKMARK_IMAGE_SERVICE_ANDROID_H_

#include "components/enhanced_bookmarks/bookmark_image_service.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

namespace chrome {
class BitmapFetcher;
}

namespace content {
class BrowserContext;
class RenderFrameHost;
class WebContents;
}

namespace enhanced_bookmarks {

class BookmarkImageServiceAndroid : public BookmarkImageService {
 public:
  explicit BookmarkImageServiceAndroid(content::BrowserContext* browserContext);

  ~BookmarkImageServiceAndroid() override;

  void RetrieveSalientImage(const GURL& page_url,
                            const GURL& image_url,
                            const std::string& referrer,
                            net::URLRequest::ReferrerPolicy referrer_policy,
                            bool update_bookmark) override;

  // Searches the current page for a salient image, if a url is found the image
  // is fetched and stored.
  void RetrieveSalientImageFromContext(
      content::RenderFrameHost* render_frame_host,
      const GURL& page_url,
      bool update_bookmark);

  // Investigates if the tab points to a bookmarked url in needs of an updated
  // image. If it is, invokes RetrieveSalientImageFromContext() for the relevant
  // urls.
  void FinishSuccessfulPageLoadForTab(content::WebContents* web_contents,
                                      bool update_bookmark);

 private:
  void RetrieveSalientImageFromContextCallback(const GURL& page_url,
                                               bool update_bookmark,
                                               const base::Value* result);

  gfx::Image ResizeImage(gfx::Image image) override;

  content::BrowserContext* browser_context_;
  // The script injected in a page to extract image urls.
  base::string16 script_;
  // Maximum size for retrieved salient images in pixels. This is used when
  // resizing an image.
  gfx::Size max_size_;

  class BitmapFetcherHandler : private chrome::BitmapFetcherDelegate {
   public:
    explicit BitmapFetcherHandler(BookmarkImageServiceAndroid* service,
                                  const GURL& image_url)
        : service_(service), bitmap_fetcher_(image_url, this) {}
    void Start(content::BrowserContext* browser_context,
               const std::string& referrer,
               net::URLRequest::ReferrerPolicy referrer_policy,
               int load_flags,
               bool update_bookmark,
               const GURL& page_url);
    void OnFetchComplete(const GURL url, const SkBitmap* bitmap) override;

   protected:
    ~BitmapFetcherHandler() override {}

   private:
    BookmarkImageServiceAndroid* service_;
    chrome::BitmapFetcher bitmap_fetcher_;
    bool update_bookmark_;
    GURL page_url_;

    DISALLOW_COPY_AND_ASSIGN(BitmapFetcherHandler);
  };

  DISALLOW_COPY_AND_ASSIGN(BookmarkImageServiceAndroid);
};

}  // namespace enhanced_bookmarks

#endif  // CHROME_BROWSER_ENHANCED_BOOKMARKS_ANDROID_BOOKMARK_IMAGE_SERVICE_ANDROID_H_
