// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#define CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
#pragma once

#include "content/common/content_export.h"
#include "net/url_request/url_fetcher.h"

namespace content {

class URLFetcherDelegate;

// Extend net::URLFetcher to add content-specific methods.
//
// TODO(akalin): Move some more content-specific methods from
// net::URLFetcher.
class CONTENT_EXPORT URLFetcher : public net::URLFetcher {
 public:
  // TODO(akalin): Move the static functions to net::URLFetcher.

  // |url| is the URL to send the request to.
  // |request_type| is the type of request to make.
  // |d| the object that will receive the callback on fetch completion.
  static URLFetcher* Create(const GURL& url,
                            RequestType request_type,
                            URLFetcherDelegate* d);

  // Like above, but if there's a URLFetcherFactory registered with the
  // implementation it will be used. |id| may be used during testing to identify
  // who is creating the URLFetcher.
  static URLFetcher* Create(int id,
                            const GURL& url,
                            RequestType request_type,
                            URLFetcherDelegate* d);

  // Cancels all existing URLFetchers.  Will notify the URLFetcherDelegates.
  // Note that any new URLFetchers created while this is running will not be
  // cancelled.  Typically, one would call this in the CleanUp() method of an IO
  // thread, so that no new URLRequests would be able to start on the IO thread
  // anyway.  This doesn't prevent new URLFetchers from trying to post to the IO
  // thread though, even though the task won't ever run.
  static void CancelAll();

  // Normally interception is disabled for URLFetcher, but you can use this
  // to enable it for tests. Also see ScopedURLFetcherFactory for another way
  // of testing code that uses an URLFetcher.
  static void SetEnableInterceptionForTests(bool enabled);

  // Mark URLRequests started by the URLFetcher to stem from the given render
  // view.
  virtual void AssociateWithRenderView(const GURL& first_party_for_cookies,
                                       int render_process_id,
                                       int render_view_id) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_FETCHER_H_
