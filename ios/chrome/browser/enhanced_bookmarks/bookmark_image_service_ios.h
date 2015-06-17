// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_IOS_H_
#define IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_IOS_H_

#include "components/enhanced_bookmarks/bookmark_image_service.h"

#import <UIKit/UIKit.h>

#include "base/containers/mru_cache.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "components/enhanced_bookmarks/image_record.h"
#include "ios/chrome/browser/net/image_fetcher.h"

@protocol CRWJSInjectionEvaluator;
class GURL;

namespace web {
class NavigationItem;
}

class BookmarkImageServiceIOS
    : public enhanced_bookmarks::BookmarkImageService {
 public:
  explicit BookmarkImageServiceIOS(
      const base::FilePath& path,
      enhanced_bookmarks::EnhancedBookmarkModel* enhanced_bookmark_model,
      net::URLRequestContextGetter* context,
      scoped_refptr<base::SequencedWorkerPool> pool);
  BookmarkImageServiceIOS(
      scoped_ptr<ImageStore> store,
      enhanced_bookmarks::EnhancedBookmarkModel* enhanced_bookmark_model,
      net::URLRequestContextGetter* context,
      scoped_refptr<base::SequencedWorkerPool> pool);
  ~BookmarkImageServiceIOS() override;

  // Searches the pageContext for a salient image, if a url is found the image
  // is fetched and stored.
  void RetrieveSalientImageFromContext(id<CRWJSInjectionEvaluator> page_context,
                                       const GURL& page_url,
                                       bool update_bookmark);

  // Invokes the superclass SalientImageForUrl, then resizes and optionally
  // darkens the image.
  void SalientImageResizedForUrl(const GURL& page_url,
                                 const CGSize size,
                                 bool darkened,
                                 const ImageCallback& callback);

  // Investigates if the newly visited page corresponding to |navigation_item|
  // and |original_url| points to a bookmarked url in needs of an updated image.
  // If it is, invokes RetrieveSalientImageFromContext() for the relevant urls
  // with |page_context|.
  void FinishSuccessfulPageLoadForNativationItem(
      id<CRWJSInjectionEvaluator> page_context,
      web::NavigationItem* navigation_item,
      const GURL& original_url);

 private:
  // Resizes large images to proper size that fits device display. This method
  // should _not_ run on the UI thread.
  scoped_ptr<gfx::Image> ResizeImage(const gfx::Image& image) override;

  // Retrieves a salient image for a given pageUrl by downloading the image in
  // one of the bookmarks.
  void RetrieveSalientImage(const GURL& page_url,
                            const GURL& image_url,
                            const std::string& referrer,
                            net::URLRequest::ReferrerPolicy referrer_policy,
                            bool update_bookmark) override;

  // Caches the returned image before returning it.
  void ReturnAndCache(
      GURL page_url,
      CGSize size,
      bool darkened,
      ImageCallback callback,
      scoped_refptr<enhanced_bookmarks::ImageRecord> image_record);

  // A cache to store the most recently found images, this allows serving those
  // images synchronously.
  class MRUKey;
  typedef scoped_refptr<enhanced_bookmarks::ImageRecord> MRUValue;
  scoped_ptr<base::MRUCache<MRUKey, MRUValue>> cache_;
  // The helper that actually fetches all those images.
  scoped_ptr<image_fetcher::ImageFetcher> imageFetcher_;
  // The script injected in a page to extract the salient image.
  base::scoped_nsobject<NSString> script_;
  // The pool used by the bookmark image service to run resize operations.
  scoped_refptr<base::SequencedWorkerPool> pool_;
  // Must be last data member.
  base::WeakPtrFactory<BookmarkImageServiceIOS> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(BookmarkImageServiceIOS);
};

#endif  // IOS_CHROME_BROWSER_ENHANCED_BOOKMARKS_BOOKMARK_IMAGE_SERVICE_IOS_H_
