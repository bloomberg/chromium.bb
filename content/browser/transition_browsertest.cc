// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "content/browser/loader/cross_site_resource_handler.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/transition_request_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_resource_dispatcher_host_delegate.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"

namespace content {

class TransitionBrowserTest : public ContentBrowserTest {
 public:
  TransitionBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TransitionBrowserTest);
};

class TransitionBrowserTestObserver
    : public WebContentsObserver,
      public ShellResourceDispatcherHostDelegate {
 public:
  TransitionBrowserTestObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        request_(NULL),
        did_defer_response_(false),
        is_transition_request_(false) {
  }

  virtual void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      int child_id,
      int route_id,
      ScopedVector<ResourceThrottle>* throttles) OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    ShellResourceDispatcherHostDelegate::RequestBeginning(request,
                                                          resource_context,
                                                          appcache_service,
                                                          resource_type,
                                                          child_id,
                                                          route_id,
                                                          throttles);
    request_ = request;

    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request_);

    if (is_transition_request_) {
      TransitionRequestManager::GetInstance()->AddPendingTransitionRequestData(
          child_id, info->GetRenderFrameID(), "*", "", "");
    }
  }

  virtual void OnResponseStarted(
      net::URLRequest* request,
      ResourceContext* resource_context,
      ResourceResponse* response,
      IPC::Sender* sender) OVERRIDE {
    ResourceRequestInfoImpl* info =
        ResourceRequestInfoImpl::ForRequest(request_);

    did_defer_response_ = info->cross_site_handler()->did_defer_for_testing();
  }

  void set_pending_transition_request(bool is_transition_request) {
    is_transition_request_ = is_transition_request;
  }

  bool did_defer_response() const { return did_defer_response_; }

 private:
  net::URLRequest* request_;
  bool did_defer_response_;
  bool is_transition_request_;
};

// This tests that normal navigations don't defer at first response.
IN_PROC_BROWSER_TEST_F(TransitionBrowserTest,
                       NormalNavigationNotDeferred) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  scoped_ptr<TransitionBrowserTestObserver> observer(
      new TransitionBrowserTestObserver(shell()->web_contents()));

  ResourceDispatcherHost::Get()->SetDelegate(observer.get());

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  EXPECT_FALSE(observer->did_defer_response());
}

// This tests that when a navigation transition is detected, the response is
// deferred.
IN_PROC_BROWSER_TEST_F(TransitionBrowserTest,
                       TransitionNavigationIsDeferred) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  scoped_ptr<TransitionBrowserTestObserver> observer(
      new TransitionBrowserTestObserver(shell()->web_contents()));

  ResourceDispatcherHost::Get()->SetDelegate(observer.get());
  observer->set_pending_transition_request(true);

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(observer->did_defer_response());
}

// This tests that the renderer is reused between the outgoing and transition.
IN_PROC_BROWSER_TEST_F(TransitionBrowserTest,
                       TransitionNavigationSharesRenderer) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  int outgoing_process_id =
      shell()->web_contents()->GetRenderProcessHost()->GetID();

  WebContents::CreateParams create_params(
      shell()->web_contents()->GetBrowserContext(),
      shell()->web_contents()->GetSiteInstance());
  scoped_ptr<WebContents> transition_web_contents(
      WebContents::Create(create_params));

  GURL about_blank(url::kAboutBlankURL);
  NavigationController::LoadURLParams params(about_blank);
  transition_web_contents->GetController().LoadURLWithParams(params);
  transition_web_contents->Focus();

  WaitForLoadStop(transition_web_contents.get());

  int transition_process_id =
      transition_web_contents->GetRenderProcessHost()->GetID();

  EXPECT_EQ(outgoing_process_id, transition_process_id);
}

}  // namespace content
