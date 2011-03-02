// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#define CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#pragma once

#include "chrome/common/notification_registrar.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "webkit/glue/webpreferences.h"

class Profile;
class TestRenderViewHost;

// Subclass TabContents to ensure it creates TestRenderViewHosts and does
// not do anything involving views.
class TestTabContents : public TabContents {
 public:
  // The render view host factory will be passed on to the
  TestTabContents(Profile* profile, SiteInstance* instance);

  TestRenderViewHost* pending_rvh() const;

  // State accessor.
  bool cross_navigation_pending() {
    return render_manager_.cross_navigation_pending_;
  }

  // Overrides TabContents::ShouldTransitionCrossSite so that we can test both
  // alternatives without using command-line switches.
  bool ShouldTransitionCrossSite() { return transition_cross_site; }

  // Overrides TabContents::Observe.  We are listening to infobar related
  // notifications so we can call InfoBarClosed() on the infobar delegates to
  // prevent them from leaking.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Promote DidNavigate to public.
  void TestDidNavigate(RenderViewHost* render_view_host,
                       const ViewHostMsg_FrameNavigate_Params& params) {
    DidNavigate(render_view_host, params);
  }

  // Promote GetWebkitPrefs to public.
  WebPreferences TestGetWebkitPrefs() {
    return GetWebkitPrefs();
  }

  // Prevent interaction with views.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host);
  virtual void UpdateRenderViewSizeForRenderManager() {}

  // Returns a clone of this TestTabContents. The returned object is also a
  // TestTabContents. The caller owns the returned object.
  virtual TabContents* Clone();

  // Creates a pending navigation to the given URL with the default parameters
  // and then commits the load with a page ID one larger than any seen. This
  // emulates what happens on a new navigation.
  void NavigateAndCommit(const GURL& url);

  // Simulates the appropriate RenderView (pending if any, current otherwise)
  // sending a navigate notification for the NavigationController pending entry.
  void CommitPendingNavigation();

  // Simulates the current RVH notifying that it has unloaded so that the
  // pending RVH navigation can proceed.
  // Does nothing if no cross-navigation is pending.
  void ProceedWithCrossSiteNavigation();

  // Set by individual tests.
  bool transition_cross_site;

  NotificationRegistrar registrar_;
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
