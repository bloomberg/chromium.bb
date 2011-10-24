// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
#pragma once

#include "content/common/content_export.h"

class URLFetcher;

namespace content {
// A delegate interface for users of URLFetcher.
class CONTENT_EXPORT URLFetcherDelegate {
 public:
  // This will be called when the URL has been fetched, successfully or not.
  // Use accessor methods on |source| to get the results.
  virtual void OnURLFetchComplete(const URLFetcher* source) = 0;

 protected:
  virtual ~URLFetcherDelegate() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_DELEGATE_H_
