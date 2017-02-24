// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_WEB_STATE_NAVIGATION_CONTEXT_H_
#define IOS_WEB_PUBLIC_WEB_STATE_NAVIGATION_CONTEXT_H_

class GURL;

namespace web {

class WebState;

// Tracks information related to a single navigation. A NavigationContext is
// provided to WebStateObserver methods to allow observers to track specific
// navigations and their details. Observers should clear any references to a
// NavigationContext at the time of WebStateObserver::DidFinishNavigation, just
// before the handle is destroyed.
class NavigationContext {
 public:
  // The WebState the navigation is taking place in.
  virtual WebState* GetWebState() = 0;

  // The URL the WebState is navigating to.
  virtual const GURL& GetUrl() const = 0;

  // Whether the navigation happened in the same page. Examples of same page
  // navigations are:
  // * reference fragment navigations
  // * pushState/replaceState
  // * same page history navigation
  virtual bool IsSamePage() const = 0;

  // Whether the navigation resulted in an error page.
  virtual bool IsErrorPage() const = 0;

  virtual ~NavigationContext() {}
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_WEB_STATE_NAVIGATION_CONTEXT_H_
