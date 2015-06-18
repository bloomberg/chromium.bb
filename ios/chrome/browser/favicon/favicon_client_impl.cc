// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/favicon/favicon_client_impl.h"

#include "base/logging.h"

FaviconClientImpl::FaviconClientImpl() {
}

FaviconClientImpl::~FaviconClientImpl() {
}

bool FaviconClientImpl::IsNativeApplicationURL(const GURL& url) {
  return false;
}

base::CancelableTaskTracker::TaskId
FaviconClientImpl::GetFaviconForNativeApplicationURL(
    const GURL& url,
    const std::vector<int>& desired_sizes_in_pixel,
    const favicon_base::FaviconResultsCallback& callback,
    base::CancelableTaskTracker* tracker) {
  NOTREACHED();
  return base::CancelableTaskTracker::kBadTaskId;
}
