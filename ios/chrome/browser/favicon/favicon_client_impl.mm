// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/favicon_client_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/cancelable_task_tracker.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

void RunFaviconCallbackIfNotCanceled(
    const base::CancelableTaskTracker::IsCanceledCallback& is_canceled_cb,
    const favicon_base::FaviconResultsCallback& original_callback,
    const std::vector<favicon_base::FaviconRawBitmapResult>& results) {
  if (!is_canceled_cb.Run()) {
    original_callback.Run(results);
  }
}

}  // namespace

FaviconClientImpl::FaviconClientImpl(ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

FaviconClientImpl::~FaviconClientImpl() {}

bool FaviconClientImpl::IsNativeApplicationURL(const GURL& url) {
  return url.SchemeIs(kChromeUIScheme);
}

base::CancelableTaskTracker::TaskId
FaviconClientImpl::GetFaviconForNativeApplicationURL(
    const GURL& url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(tracker);
  DCHECK(IsNativeApplicationURL(url));
  base::CancelableTaskTracker::IsCanceledCallback is_canceled_cb;
  base::CancelableTaskTracker::TaskId task_id =
      tracker->NewTrackedTaskId(&is_canceled_cb);
  if (task_id != base::CancelableTaskTracker::kBadTaskId) {
    ios::GetChromeBrowserProvider()->GetFaviconForURL(
        browser_state_, url, desired_sizes_in_pixel,
        base::Bind(&RunFaviconCallbackIfNotCanceled, is_canceled_cb, callback));
  }
  return task_id;
}
