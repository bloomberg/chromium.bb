// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_
#define COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_

#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

namespace favicon_base {
struct FaviconRawBitmapResult;
}

namespace favicon {

class FaviconService;

// The large icon service provides methods to access large icons. It relies on
// the favicon service.
class LargeIconService : public KeyedService {
 public:
  explicit LargeIconService(FaviconService* favicon_service);

  ~LargeIconService() override;

  // Requests the best large icon for the page at |page_url| given the requested
  // |desired_size_in_pixel|. If no good large icon can be found, returns the
  // fallback style to use, for which the background is set to the dominant
  // color of a smaller icon when one is available. This function returns the
  // style of the fallback icon rather than the rendered version so that clients
  // can render the icon themselves.
  base::CancelableTaskTracker::TaskId GetLargeIconOrFallbackStyle(
    const GURL& page_url,
    int desired_size_in_pixel,
    const favicon_base::LargeIconCallback& callback,
    base::CancelableTaskTracker* tracker);

 private:
  // Intermediate callback for GetLargeIconOrFallbackStyle(). Ensures the large
  // icon is at least the desired size, if not compute the icon fallback style
  // and use it to invoke |callback|.
  void RunLargeIconCallback(
      const favicon_base::LargeIconCallback& callback,
      int desired_size_in_pixel,
      const favicon_base::FaviconRawBitmapResult& bitmap_result);

  FaviconService* favicon_service_;

  // A pre-populated list of the types of icon files to consider when looking
  // for large icons. This is an optimization over populating an icon type
  // vector on each request.
  std::vector<int> large_icon_types_;

  DISALLOW_COPY_AND_ASSIGN(LargeIconService);
};

}  // namespace favicon

#endif  // COMPONENTS_FAVICON_CORE_LARGE_ICON_SERVICE_H_
