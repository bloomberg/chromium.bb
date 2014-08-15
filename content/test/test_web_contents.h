// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_WEB_CONTENTS_H_
#define CONTENT_TEST_TEST_WEB_CONTENTS_H_

#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"

class SiteInstanceImpl;

namespace content {

class RenderViewHost;
class TestRenderViewHost;
class WebContentsTester;

// Subclass WebContentsImpl to ensure it creates TestRenderViewHosts
// and does not do anything involving views.
class TestWebContents : public WebContentsImpl, public WebContentsTester {
 public:
  virtual ~TestWebContents();

  static TestWebContents* Create(BrowserContext* browser_context,
                                 SiteInstance* instance);

  // WebContentsImpl overrides (returning the same values, but in Test* types)
  virtual TestRenderFrameHost* GetMainFrame() OVERRIDE;
  virtual TestRenderViewHost* GetRenderViewHost() const OVERRIDE;

  // WebContentsTester implementation.
  virtual void CommitPendingNavigation() OVERRIDE;
  virtual TestRenderFrameHost* GetPendingMainFrame() const OVERRIDE;
  virtual void NavigateAndCommit(const GURL& url) OVERRIDE;
  virtual void TestSetIsLoading(bool value) OVERRIDE;
  virtual void ProceedWithCrossSiteNavigation() OVERRIDE;
  virtual void TestDidNavigate(RenderFrameHost* render_frame_host,
                               int page_id,
                               const GURL& url,
                               PageTransition transition) OVERRIDE;
  virtual void TestDidNavigateWithReferrer(RenderFrameHost* render_frame_host,
                                           int page_id,
                                           const GURL& url,
                                           const Referrer& referrer,
                                           PageTransition transition) OVERRIDE;
  virtual WebPreferences TestComputeWebkitPrefs() OVERRIDE;

  // State accessor.
  bool cross_navigation_pending() {
    return GetRenderManager()->cross_navigation_pending_;
  }

  // Prevent interaction with views.
  virtual bool CreateRenderViewForRenderManager(
      RenderViewHost* render_view_host,
      int opener_route_id,
      int proxy_routing_id,
      bool for_main_frame) OVERRIDE;
  virtual void UpdateRenderViewSizeForRenderManager() OVERRIDE {}

  // Returns a clone of this TestWebContents. The returned object is also a
  // TestWebContents. The caller owns the returned object.
  virtual WebContents* Clone() OVERRIDE;

  // Allow mocking of the RenderViewHostDelegateView.
  virtual RenderViewHostDelegateView* GetDelegateView() OVERRIDE;
  void set_delegate_view(RenderViewHostDelegateView* view) {
    delegate_view_override_ = view;
  }

  // Allows us to simulate this tab having an opener.
  void SetOpener(TestWebContents* opener);

  // Allows us to simulate that a contents was created via CreateNewWindow.
  void AddPendingContents(TestWebContents* contents);

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

  void TestDidFinishLoad(const GURL& url);
  void TestDidFailLoadWithError(const GURL& url,
                                int error_code,
                                const base::string16& error_description);

 protected:
  // The deprecated WebContentsTester still needs to subclass this.
  explicit TestWebContents(BrowserContext* browser_context);

 private:
  // WebContentsImpl overrides
  virtual void CreateNewWindow(
      int render_process_id,
      int route_id,
      int main_frame_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace) OVERRIDE;
  virtual void CreateNewWidget(int render_process_id,
                               int route_id,
                               blink::WebPopupType popup_type) OVERRIDE;
  virtual void CreateNewFullscreenWidget(int render_process_id,
                                         int route_id) OVERRIDE;
  virtual void ShowCreatedWindow(int route_id,
                                 WindowOpenDisposition disposition,
                                 const gfx::Rect& initial_pos,
                                 bool user_gesture) OVERRIDE;
  virtual void ShowCreatedWidget(int route_id,
                                 const gfx::Rect& initial_pos) OVERRIDE;
  virtual void ShowCreatedFullscreenWidget(int route_id) OVERRIDE;


  RenderViewHostDelegateView* delegate_view_override_;

  // Expectations for arguments of |SetHistoryLengthAndPrune()|.
  bool expect_set_history_length_and_prune_;
  scoped_refptr<const SiteInstanceImpl>
    expect_set_history_length_and_prune_site_instance_;
  int expect_set_history_length_and_prune_history_length_;
  int32 expect_set_history_length_and_prune_min_page_id_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_WEB_CONTENTS_H_
