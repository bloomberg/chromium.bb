// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/frame_host/form_submission_throttle.h"

#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace content {

class FormSubmissionBrowserTest : public ContentBrowserTest {
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(FormSubmissionBrowserTest,
                       CheckContentSecurityPolicyFormAction) {
  // The FormSubmissionThrottle isn't used without PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  const struct {
    GURL main_page_url;
    GURL form_page_url;
    NavigationThrottle::ThrottleAction start_expectation;
    NavigationThrottle::ThrottleAction redirect_expectation;
  } kTestCases[] = {
      // Form submissions is allowed by default when there is no CSP.
      {
          embedded_test_server()->GetURL(
              "/form_submission_throttle/no_csp.html"),
          embedded_test_server()->GetURL("/simple_page.html"),
          NavigationThrottle::PROCEED,  // start expectation.
          NavigationThrottle::PROCEED   // redirect expectation.
      },

      // No form submission is allowed when the calling RenderFrameHost's CSP
      // is "form-action 'none'".
      {
          embedded_test_server()->GetURL(
              "/form_submission_throttle/form_action_none.html"),
          embedded_test_server()->GetURL("/simple_page.html"),
          NavigationThrottle::CANCEL,  // start expectation.
          NavigationThrottle::CANCEL   // redirect expectation.
      },

      // The path of the source-expression is only enforced when there is no
      // redirection. By using this behavior, this test can check a case where
      // the request is canceled in WillStartRequest() but not in
      // WillRedirectRequest().
      // See https://www.w3.org/TR/CSP2/#source-list-paths-and-redirects for
      // details.
      {
          embedded_test_server()->GetURL(
              "/form_submission_throttle/form_action_with_path.html"),
          embedded_test_server()->GetURL("/not_the_file.html"),
          NavigationThrottle::CANCEL,  // start expectation.
          NavigationThrottle::PROCEED  // redirect expectation.
      },
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(testing::Message()
                 << std::endl
                 << "main_page_url = " << test.main_page_url << std::endl
                 << "form_page_url = " << test.form_page_url << std::endl);

    // Load the main page.
    EXPECT_TRUE(NavigateToURL(shell(), test.main_page_url));

    // Build a new form submission navigation.
    FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                              ->GetFrameTree()
                              ->root();
    std::unique_ptr<NavigationHandle> handle = NavigationHandleImpl::Create(
        test.form_page_url,      // url
        std::vector<GURL>(),     // redirect chain
        root,                    // frame_tree_node
        true,                    // is_renderer_initiated
        false,                   // is_same_page
        base::TimeTicks::Now(),  // navigation_start
        0,                       // pending_nav_entry_id
        false,                   // started_from_context_menu
        CSPDisposition::CHECK,   // should_check_main_world_csp
        true);                   // is_form_submission

    // Test the expectations with a FormSubmissionThrottle.
    std::unique_ptr<NavigationThrottle> throttle =
        FormSubmissionThrottle::MaybeCreateThrottleFor(handle.get());
    ASSERT_TRUE(throttle);
    EXPECT_EQ(test.start_expectation, throttle->WillStartRequest());
    EXPECT_EQ(test.redirect_expectation, throttle->WillRedirectRequest());
  }
}

IN_PROC_BROWSER_TEST_F(FormSubmissionBrowserTest,
                       CheckContentSecurityPolicyFormActionBypassCSP) {
  // The FormSubmissionThrottle isn't used without PlzNavigate.
  if (!IsBrowserSideNavigationEnabled())
    return;

  GURL main_url = embedded_test_server()->GetURL(
      "/form_submission_throttle/form_action_none.html");
  GURL form_url = embedded_test_server()->GetURL("/simple_page.html");

  // Load the main page.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Build a new form submission navigation.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  std::unique_ptr<NavigationHandle> handle = NavigationHandleImpl::Create(
      form_url,                      // url
      std::vector<GURL>(),           // redirect chain
      root,                          // frame_tree_node
      true,                          // is_renderer_initiated
      false,                         // is_same_page
      base::TimeTicks::Now(),        // navigation_start
      0,                             // pending_nav_entry_id
      false,                         // started_from_context_menu
      CSPDisposition::DO_NOT_CHECK,  // should_check_main_world_csp
      true);                         // is_form_submission

  // Test that the navigation is allowed because "should_by_pass_main_world_csp"
  // is true, even if it is a form submission and the policy is
  // "form-action 'none'".
  std::unique_ptr<NavigationThrottle> throttle =
      FormSubmissionThrottle::MaybeCreateThrottleFor(handle.get());
  ASSERT_TRUE(throttle);
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillStartRequest());
  EXPECT_EQ(NavigationThrottle::PROCEED, throttle->WillRedirectRequest());
}

}  // namespace content
