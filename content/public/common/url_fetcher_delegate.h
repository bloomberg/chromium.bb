// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"

namespace content {

class URLFetcher;

// A delegate interface for users of URLFetcher.
class CONTENT_EXPORT URLFetcherDelegate {
 public:
  // This will be called when the URL has been fetched, successfully or not.
  // Use accessor methods on |source| to get the results.
  virtual void OnURLFetchComplete(const URLFetcher* source) = 0;

  // This will be called when some part of the response are read. |current|
  // denotes the sum of bytes received up to the call, and |total| is the
  // expected total size of the response (or -1 if not determined).
  virtual void OnURLFetchDownloadProgress(const URLFetcher* source,
                                          int64 current, int64 total) {}

 protected:
  virtual ~URLFetcherDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
