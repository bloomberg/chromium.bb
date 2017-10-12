// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_NAVIGATION_THROTTLE_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_NAVIGATION_THROTTLE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_throttle.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace extensions {

class Extension;

// This class creates navigation throttles that redirect URLs to Bookmark Apps
// if the URLs are in scope of the Bookmark App.
class BookmarkAppNavigationThrottle : public content::NavigationThrottle {
 public:
  static std::unique_ptr<content::NavigationThrottle> MaybeCreateThrottleFor(
      content::NavigationHandle* navigation_handle);

  explicit BookmarkAppNavigationThrottle(
      content::NavigationHandle* navigation_handle);
  ~BookmarkAppNavigationThrottle() override;

  // content::NavigationThrottle:
  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  const char* GetNameForLogging() override;

 private:
  content::NavigationThrottle::ThrottleCheckResult CheckNavigation();
  void OpenBookmarkApp(scoped_refptr<const Extension> bookmark_app);
  void CloseWebContents();

  base::WeakPtrFactory<BookmarkAppNavigationThrottle> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppNavigationThrottle);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_NAVIGATION_THROTTLE_H_
