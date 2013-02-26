// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_
#define CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_

#include "content/public/common/page_transition_types.h"

class GURL;

namespace webkit_glue {
struct WebPreferences;
}

namespace content {

class BrowserContext;
struct Referrer;
class RenderViewHost;
class SiteInstance;
class WebContents;

// This interface allows embedders of content/ to write tests that depend on a
// test version of WebContents.  This interface can be retrieved from any
// WebContents that was retrieved via a call to
// RenderViewHostTestHarness::GetWebContents() (directly or indirectly) or
// constructed explicitly via one of the WebContentsTester::Create...  methods.
//
// Tests within content/ can directly static_cast WebContents objects retrieved
// or created as described above to TestWebContents.
//
// Design note: We considered two alternatives to this separate test interface
// approach:
//
// a) Define a TestWebContents interface that inherits from WebContents, and
// have the concrete TestWebContents inherit from it as well as from
// WebContentsImpl.  This approach was discarded as it introduces a diamond
// inheritance pattern, which means we wouldn't be e.g. able to downcast from
// WebContents to WebContentsImpl using static_cast.
//
// b) Define a TestWebContents interface that inherits from WebContents, and
// have the concrete TestWebContents implement it, using composition of a
// WebContentsImpl to implement most methods.  This approach was discarded as
// there is a fundamental assumption in content/ that a WebContents* can be
// downcast to a WebContentsImpl*, and this wouldn't be true for TestWebContents
// objects.
class WebContentsTester {
 public:
  // Retrieves a WebContentsTester to drive tests of the specified WebContents.
  // As noted above you need to be sure the 'contents' object supports testing,
  // i.e. is either created using one of the Create... functions below, or is
  // retrieved via RenderViewHostTestHarness::GetWebContents().
  static WebContentsTester* For(WebContents* contents);

  // Creates a WebContents enabled for testing.
  static WebContents* CreateTestWebContents(
      BrowserContext* browser_context,
      SiteInstance* instance);

  // Deprecated.  Creates a WebContents enabled for testing, that
  // counts the number of times SetFocusToLocationBar is called.
  static WebContents*
      CreateTestWebContentsCountSetFocusToLocationBar(
          BrowserContext* browser_context,
          SiteInstance* instance);

  // Simulates the appropriate RenderView (pending if any, current otherwise)
  // sending a navigate notification for the NavigationController pending entry.
  virtual void CommitPendingNavigation() = 0;

  // Only implementations retrieved via the deprecated
  // CreateTestWebContentsFor... methods above will implement this
  // method.  It retrieves the number of times the focus-related calls
  // in question have been made.
  virtual int GetNumberOfFocusCalls() = 0;

  virtual RenderViewHost* GetPendingRenderViewHost() const = 0;

  // Creates a pending navigation to the given URL with the default parameters
  // and then commits the load with a page ID one larger than any seen. This
  // emulates what happens on a new navigation.
  virtual void NavigateAndCommit(const GURL& url) = 0;

  // Sets the loading state to the given value.
  virtual void TestSetIsLoading(bool value) = 0;

  // Simulates the current RVH notifying that it has unloaded so that the
  // pending RVH navigation can proceed.
  // Does nothing if no cross-navigation is pending.
  virtual void ProceedWithCrossSiteNavigation() = 0;

  virtual void TestDidNavigate(RenderViewHost* render_view_host,
                               int page_id,
                               const GURL& url,
                               PageTransition transition) = 0;

  virtual void TestDidNavigateWithReferrer(
      RenderViewHost* render_view_host,
      int page_id,
      const GURL& url,
      const Referrer& referrer,
      PageTransition transition) = 0;

  // Promote GetWebkitPrefs to public.
  virtual webkit_glue::WebPreferences TestGetWebkitPrefs() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_WEB_CONTENTS_TESTER_H_
