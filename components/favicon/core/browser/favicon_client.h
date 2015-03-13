// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_
#define COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_

#include <vector>

#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/favicon_base/favicon_callback.h"
#include "components/keyed_service/core/keyed_service.h"

class GURL;

// This class abstracts operations that depend on the embedder's environment,
// e.g. Chrome.
class FaviconClient : public KeyedService {
 public:
  // Returns true if the specified URL is bookmarked.
  virtual bool IsBookmarked(const GURL& url) = 0;

  // Returns true if the specified URL is a native application page URL.
  // If this returns true the favicon for the page must be fetched using
  // GetFaviconForNativeApplicationURL().
  virtual bool IsNativeApplicationURL(const GURL& url) = 0;

  // Requests the favicon for a native application page URL for the sizes
  // specified by |desired_sizes_in_pixel|. Returns a TaskId to use to cancel
  // the request using |tracker| or kBadTaskId if the request cannot be
  // scheduled. |callback| will be called with the favicon results.
  virtual base::CancelableTaskTracker::TaskId GetFaviconForNativeApplicationURL(
      const GURL& url,
      const std::vector<int>& desired_sizes_in_pixel,
      const favicon_base::FaviconResultsCallback& callback,
      base::CancelableTaskTracker* tracker) = 0;

 protected:
  FaviconClient() {}
  ~FaviconClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FaviconClient);
};

#endif  // COMPONENTS_FAVICON_CORE_BROWSER_FAVICON_CLIENT_H_
