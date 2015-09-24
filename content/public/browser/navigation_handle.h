// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
#define CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_

#include "content/common/content_export.h"
#include "net/base/net_errors.h"

class GURL;

namespace content {
class RenderFrameHost;

// A NavigationHandle tracks information related to a single navigation.
class CONTENT_EXPORT NavigationHandle {
 public:
  virtual ~NavigationHandle() {}

  // The URL the frame is navigating to. This may change during the navigation
  // when encountering a server redirect.
  virtual const GURL& GetURL() const = 0;

  // The net error code if an error happened prior to commit. Otherwise it will
  // be net::OK.
  virtual net::Error GetNetErrorCode() const = 0;

  // Whether the navigation is taking place in the main frame or in a subframe.
  virtual bool IsInMainFrame() const = 0;

  // Returns the RenderFrameHost this navigation is taking place in. This can
  // only be accessed after the navigation is ready to commit.
  virtual RenderFrameHost* GetRenderFrameHost() = 0;

  // Whether the navigation happened in the same page. This is only known
  // after the navigation has committed. It is an error to call this method
  // before the navigation has committed.
  virtual bool IsSamePage() = 0;

  // Whether the navigation has committed. This returns true for either
  // successful commits or error pages that replace the previous page
  // (distinguished by |IsErrorPage|), and false for errors that leave the user
  // on the previous page.
  virtual bool HasCommitted() = 0;

  // Whether the navigation resulted in an error page.
  virtual bool IsErrorPage() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_NAVIGATION_HANDLE_H_
