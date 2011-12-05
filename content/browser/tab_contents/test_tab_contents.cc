// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tab_contents/test_tab_contents.h"

#include <utility>

#include "content/browser/browser_url_handler.h"
#include "content/browser/renderer_host/mock_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/common/view_messages.h"
#include "content/public/common/page_transition_types.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/webkit_glue.h"

TestTabContents::TestTabContents(content::BrowserContext* browser_context,
                                 SiteInstance* instance)
    : TabContents(browser_context, instance, MSG_ROUTING_NONE, NULL, NULL),
      transition_cross_site(false),
      delegate_view_override_(NULL),
      expect_set_history_length_and_prune_(false),
      expect_set_history_length_and_prune_site_instance_(NULL),
      expect_set_history_length_and_prune_history_length_(0),
      expect_set_history_length_and_prune_min_page_id_(-1) {
}

TestTabContents::~TestTabContents() {
}

TestRenderViewHost* TestTabContents::pending_rvh() const {
  return static_cast<TestRenderViewHost*>(
      render_manager_.pending_render_view_host_);
}

void TestTabContents::TestDidNavigate(RenderViewHost* render_view_host,
                                      int page_id,
                                      const GURL& url,
                                      content::PageTransition transition) {
  TestDidNavigateWithReferrer(render_view_host,
                              page_id,
                              url,
                              content::Referrer(),
                              transition);
}

void TestTabContents::TestDidNavigateWithReferrer(
    RenderViewHost* render_view_host,
    int page_id,
    const GURL& url,
    const content::Referrer& referrer,
    content::PageTransition transition) {
  ViewHostMsg_FrameNavigate_Params params;

  params.page_id = page_id;
  params.url = url;
  params.referrer = referrer;
  params.transition = transition;
  params.redirects = std::vector<GURL>();
  params.should_update_history = false;
  params.searchable_form_url = GURL();
  params.searchable_form_encoding = std::string();
  params.password_form = webkit_glue::PasswordForm();
  params.security_info = std::string();
  params.gesture = NavigationGestureUser;
  params.was_within_same_page = false;
  params.is_post = false;
  params.content_state = webkit_glue::CreateHistoryStateForURL(GURL(url));

  DidNavigate(render_view_host, params);
}

bool TestTabContents::CreateRenderViewForRenderManager(
    RenderViewHost* render_view_host) {
  // This will go to a TestRenderViewHost.
  render_view_host->CreateRenderView(string16());
  return true;
}

TabContents* TestTabContents::Clone() {
  TabContents* tc = new TestTabContents(
      browser_context(), SiteInstance::CreateSiteInstance(browser_context()));
  tc->controller().CopyStateFrom(controller_);
  return tc;
}

void TestTabContents::NavigateAndCommit(const GURL& url) {
  controller().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_LINK, std::string());
  GURL loaded_url(url);
  bool reverse_on_redirect = false;
  BrowserURLHandler::GetInstance()->RewriteURLIfNecessary(
      &loaded_url, browser_context(), &reverse_on_redirect);

  // LoadURL created a navigation entry, now simulate the RenderView sending
  // a notification that it actually navigated.
  CommitPendingNavigation();
}

void TestTabContents::CommitPendingNavigation() {
  // If we are doing a cross-site navigation, this simulates the current RVH
  // notifying that it has unloaded so the pending RVH is resumed and can
  // navigate.
  ProceedWithCrossSiteNavigation();
  RenderViewHost* old_rvh = render_manager_.current_host();
  TestRenderViewHost* rvh = pending_rvh();
  if (!rvh)
    rvh = static_cast<TestRenderViewHost*>(old_rvh);

  const NavigationEntry* entry = controller().pending_entry();
  DCHECK(entry);
  int page_id = entry->page_id();
  if (page_id == -1) {
    // It's a new navigation, assign a never-seen page id to it.
    page_id =
        static_cast<MockRenderProcessHost*>(rvh->process())->max_page_id() + 1;
  }
  rvh->SendNavigate(page_id, entry->url());

  // Simulate the SwapOut_ACK that fires if you commit a cross-site navigation
  // without making any network requests.
  if (old_rvh != rvh)
    old_rvh->OnSwapOutACK();
}

void TestTabContents::ProceedWithCrossSiteNavigation() {
  if (!pending_rvh())
    return;
  TestRenderViewHost* rvh = static_cast<TestRenderViewHost*>(
      render_manager_.current_host());
  rvh->SendShouldCloseACK(true);
}

RenderViewHostDelegate::View* TestTabContents::GetViewDelegate() {
  if (delegate_view_override_)
    return delegate_view_override_;
  return TabContents::GetViewDelegate();
}

void TestTabContents::ExpectSetHistoryLengthAndPrune(
    const SiteInstance* site_instance,
    int history_length,
    int32 min_page_id) {
  expect_set_history_length_and_prune_ = true;
  expect_set_history_length_and_prune_site_instance_ = site_instance;
  expect_set_history_length_and_prune_history_length_ = history_length;
  expect_set_history_length_and_prune_min_page_id_ = min_page_id;
}

void TestTabContents::SetHistoryLengthAndPrune(
    const SiteInstance* site_instance, int history_length, int32 min_page_id) {
  EXPECT_TRUE(expect_set_history_length_and_prune_);
  expect_set_history_length_and_prune_ = false;
  EXPECT_EQ(expect_set_history_length_and_prune_site_instance_, site_instance);
  EXPECT_EQ(expect_set_history_length_and_prune_history_length_,
            history_length);
  EXPECT_EQ(expect_set_history_length_and_prune_min_page_id_, min_page_id);
}
