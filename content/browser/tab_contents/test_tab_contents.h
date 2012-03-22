// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#define CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
#pragma once

#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/test/web_contents_tester.h"
#include "webkit/glue/webpreferences.h"

class SiteInstanceImpl;

namespace content {
class RenderViewHost;
class TestRenderViewHost;
class WebContentsTester;
}

// Subclass TabContents to ensure it creates TestRenderViewHosts
// and does not do anything involving views.
//
// TODO(joi): Rename TestWebContents.
// TODO(joi): Move to content namespace.
class TestTabContents : public TabContents, public content::WebContentsTester {
 public:
  TestTabContents(content::BrowserContext* browser_context,
                  content::SiteInstance* instance);
  virtual ~TestTabContents();

  // WebContentsTester implementation.
  virtual void CommitPendingNavigation() OVERRIDE;
  virtual int GetNumberOfFocusCalls() OVERRIDE;
  virtual content::RenderViewHost* GetPendingRenderViewHost() const OVERRIDE;
  virtual void NavigateAndCommit(const GURL& url) OVERRIDE;
  virtual void ProceedWithCrossSiteNavigation() OVERRIDE;
  virtual void TestDidNavigate(content::RenderViewHost* render_view_host,
                               int page_id,
                               const GURL& url,
                               content::PageTransition transition) OVERRIDE;
  virtual void TestDidNavigateWithReferrer(
      content::RenderViewHost* render_view_host,
      int page_id,
      const GURL& url,
      const content::Referrer& referrer,
      content::PageTransition transition) OVERRIDE;
  virtual WebPreferences TestGetWebkitPrefs() OVERRIDE;

  content::TestRenderViewHost* pending_test_rvh() const;

  // State accessor.
  bool cross_navigation_pending() {
    return render_manager_.cross_navigation_pending_;
  }

  // Overrides TabContents::ShouldTransitionCrossSite so that we can test both
  // alternatives without using command-line switches.
  bool ShouldTransitionCrossSite() { return transition_cross_site; }

  // Prevent interaction with views.
  virtual bool CreateRenderViewForRenderManager(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void UpdateRenderViewSizeForRenderManager() OVERRIDE {}

  // Returns a clone of this TestTabContents. The returned object is also a
  // TestTabContents. The caller owns the returned object.
  virtual content::WebContents* Clone() OVERRIDE;

  // Set by individual tests.
  bool transition_cross_site;

  // Allow mocking of the RenderViewHostDelegate::View.
  virtual content::RenderViewHostDelegate::View* GetViewDelegate() OVERRIDE;
  void set_view_delegate(content::RenderViewHostDelegate::View* view) {
    delegate_view_override_ = view;
  }

  // Establish expected arguments for |SetHistoryLengthAndPrune()|. When
  // |SetHistoryLengthAndPrune()| is called, the arguments are compared
  // with the expected arguments specified here.
  void ExpectSetHistoryLengthAndPrune(
      const content::SiteInstance* site_instance,
      int history_length,
      int32 min_page_id);

  // Compares the arguments passed in with the expected arguments passed in
  // to |ExpectSetHistoryLengthAndPrune()|.
  virtual void SetHistoryLengthAndPrune(
      const content::SiteInstance* site_instance,
      int history_length,
      int32 min_page_id) OVERRIDE;

 private:
  content::RenderViewHostDelegate::View* delegate_view_override_;

  // Expectations for arguments of |SetHistoryLengthAndPrune()|.
  bool expect_set_history_length_and_prune_;
  scoped_refptr<const SiteInstanceImpl>
    expect_set_history_length_and_prune_site_instance_;
  int expect_set_history_length_and_prune_history_length_;
  int32 expect_set_history_length_and_prune_min_page_id_;
};

#endif  // CONTENT_BROWSER_TAB_CONTENTS_TEST_TAB_CONTENTS_H_
