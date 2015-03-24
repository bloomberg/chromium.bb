// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_WEB_LOAD_PARAMS_H_
#define IOS_WEB_NAVIGATION_WEB_LOAD_PARAMS_H_

#import <Foundation/Foundation.h>

#import "base/mac/scoped_nsobject.h"
#include "ios/net/request_tracker.h"
#include "ios/web/public/referrer.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {

// Parameters for creating a tab. Most paramaters are optional, and
// can be left at the default values set by the constructor.
// TODO(stuartmorgan): Remove the dependency on RequestTracker, then move
// this into NavigationManager to parallel NavigationController's
// LoadURLParams (which this is modeled after).
struct WebLoadParams {
 public:
  // The URL to load. Must be set.
  GURL url;

  // The referrer for the load. May be empty.
  Referrer referrer;

  // The transition type for the load. Defaults to PAGE_TRANSITION_LINK.
  ui::PageTransition transition_type;

  // True for renderer-initiated navigations. This is
  // important for tracking whether to display pending URLs.
  bool is_renderer_initiated;

  // The cache mode to load with. Defaults to CACHE_NORMAL.
  net::RequestTracker::CacheMode cache_mode;

  // Any extra HTTP headers to add to the load.
  base::scoped_nsobject<NSDictionary> extra_headers;

  // Any post data to send with the load. When setting this, you should
  // generally set a Content-Type header as well.
  base::scoped_nsobject<NSData> post_data;

  // Create a new WebLoadParams with the given URL and defaults for all other
  // parameters.
  explicit WebLoadParams(const GURL& url);
  ~WebLoadParams();

  // Allow copying WebLoadParams.
  WebLoadParams(const WebLoadParams& other);
  WebLoadParams& operator=(const WebLoadParams& other);
};

}  // namespace web

#endif  // IOS_WEB_NAVIGATION_WEB_LOAD_PARAMS_H_
