// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#define CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#pragma once

#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/page_transition_types.h"
#include "webkit/glue/webpreferences.h"

class TestRenderViewHost;

// Subclass TabContents to ensure it creates TestRenderViewHosts and does
// not do anything involving views.
class TestTabContents : public TabContents {
 public:
  TestTabContents(content::BrowserContext* browser_context,
                  SiteInstance* instance);
  virtual ~TestTabContents();

  TestRenderViewHost* pending_rvh() const;

  // State accessor.
  bool cross_navigation_pending() {
    return render_manager_.cross_navigation_pending_;
  }

  // Overrides TabContents::ShouldTransitionCrossSite so that we can test both
  // alternatives without using command-line switches.
  bool ShouldTransitionCrossSite() { return transition_cross_site; }

  void TestDidNavigate(RenderViewHost* render_view_host,
                       int page_id,
                       const GURL& url,
                       content::PageTransition transition);
  void TestDidNavigateWithReferrer(RenderViewHost* render_view_host,
                                   int page_id,
                                   const GURL& url,
                                   const content::Referrer& referrer,
                                   content::PageTransition transition);

  // Promote GetWebkitPrefs to public.
  WebPreferences TestGetWebkitPrefs() {
    return GetWebkitPrefs();
  }

  // Prevent interaction with views.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host) OVERRIDE;
  virtual void UpdateRenderViewSizeForRenderManager() OVERRIDE {}

  // Returns a clone of this TestTabContents. The returned object is also a
  // TestTabContents. The caller owns the returned object.
  virtual TabContents* Clone() OVERRIDE;

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

  // Allow mocking of the RenderViewHostDelegate::View.
  virtual RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  void set_view_delegate(RenderViewHostDelegate::View* view) {
    delegate_view_override_ = view;
  }

  // Establish expected arguments for |SetHistoryLengthAndPrune()|. When
  // |SetHistoryLengthAndPrune()| is called, the arguments are compared
  // with the expected arguments specified here.
  void ExpectSetHistoryLengthAndPrune(const SiteInstance* site_instance,
                                      int history_length,
                                      int32 min_page_id);

  // Compares the arguments passed in with the expected arguments passed in
  // to |ExpectSetHistoryLengthAndPrune()|.
  virtual void SetHistoryLengthAndPrune(const SiteInstance* site_instance,
                                        int history_length,
                                        int32 min_page_id) OVERRIDE;

 private:
  RenderViewHostDelegate::View* delegate_view_override_;

  // Expectations for arguments of |SetHistoryLengthAndPrune()|.
  bool expect_set_history_length_and_prune_;
  scoped_refptr<const SiteInstance>
    expect_set_history_length_and_prune_site_instance_;
  int expect_set_history_length_and_prune_history_length_;
  int32 expect_set_history_length_and_prune_min_page_id_;
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
