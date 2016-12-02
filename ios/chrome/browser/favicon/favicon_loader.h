// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FAVICON_FAVICON_LOADER_H_
#define IOS_CHROME_BROWSER_FAVICON_FAVICON_LOADER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread_checker.h"
#include "components/favicon_base/favicon_types.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;
@class NSMutableDictionary;
@class UIImage;

namespace favicon {
class FaviconService;
}

// A class that manages asynchronously loading favicons from the favicon
// service and caching them, given a URL. This is predominately used by the
// MostVisited panel, since every other display of favicons already has a
// bitmap in the relevant data structure. There is one of these per browser
// state to avoid re-creating favicons for every instance of the NTP.
class FaviconLoader : public KeyedService {
 public:
  // Type for completion block for ImageForURL().
  typedef void (^ImageCompletionBlock)(UIImage*);

  explicit FaviconLoader(favicon::FaviconService* favicon_service);
  ~FaviconLoader() override;

  // Returns the UIImage for the favicon associated with |url|, if present.
  // |types| is a bitfield of history::IconType with the acceptable types. If
  // the icons is not present, will start an asynchronous load with the favicon
  // service and returns the default favicon (thus it will never return nil).
  // Calls |block| when the load completes with the image. If |block| is nil,
  // the load is still performed so a future call will find it in the cache.
  UIImage* ImageForURL(const GURL& url, int types, ImageCompletionBlock block);

  // Purges the cache, in response to low-memory.
  void PurgeCache();

 private:
  struct RequestData;

  // Called when the favicon load request completes. Saves image into the
  // cache. Desktop code assumes this image is in PNG format.
  void OnFaviconAvailable(
      std::unique_ptr<RequestData> request_data,
      const std::vector<favicon_base::FaviconRawBitmapResult>&
          favicon_bitmap_results);

  base::ThreadChecker thread_checker_;

  // The FaviconService used to retrieve favicon; may be null during testing.
  // Must outlive the FaviconLoader.
  favicon::FaviconService* favicon_service_;

  // Tracks tasks sent to FaviconService.
  base::CancelableTaskTracker cancelable_task_tracker_;

  // Holds cached favicons. This dictionary is populated as favicons are
  // retrieved from the FaviconService. This will be emptied during low-memory
  // conditions. Keyed by NSString of URL spec.
  NSMutableDictionary* favicon_cache_;

  DISALLOW_COPY_AND_ASSIGN(FaviconLoader);
};

#endif  // IOS_CHROME_BROWSER_FAVICON_FAVICON_LOADER_H_
