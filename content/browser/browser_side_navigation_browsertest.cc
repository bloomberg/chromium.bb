// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc_security_test_util.h"
#include "net/base/filename_util.h"
#include "net/base/load_flags.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "url/gurl.h"

namespace content {

class BrowserSideNavigationBrowserTest : public ContentBrowserTest {
 public:
  BrowserSideNavigationBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnableBrowserSideNavigation);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

// Ensure that browser initiated basic navigations work with browser side
// navigation.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       BrowserInitiatedNavigations) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()->root()->current_frame_host();

  // Perform a same site navigation.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should not have changed.
  EXPECT_EQ(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()->root()->current_frame_host());

  // Perform a cross-site navigation.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url = embedded_test_server()->GetURL("foo.com", "/title3.html");
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should have changed.
  EXPECT_NE(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()->root()->current_frame_host());
}

// Ensure that renderer initiated same-site navigations work with browser side
// navigation.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       RendererInitiatedSameSiteNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/simple_links.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()->root()->current_frame_host();

  // Simulate clicking on a same-site link.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title2.html"));
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickSameSiteLink());",
        &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should not have changed.
  EXPECT_EQ(initial_rfh, static_cast<WebContentsImpl*>(shell()->web_contents())
                             ->GetFrameTree()->root()->current_frame_host());
}

// Ensure that renderer initiated cross-site navigations work with browser side
// navigation.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       RendererInitiatedCrossSiteNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/simple_links.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  RenderFrameHost* initial_rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetFrameTree()->root()->current_frame_host();

  // Simulate clicking on a cross-site link.
  {
    TestNavigationObserver observer(shell()->web_contents());
    const char kReplacePortNumber[] =
      "window.domAutomationController.send(setPortNumber(%d));";
    uint16_t port_number = embedded_test_server()->port();
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), base::StringPrintf(kReplacePortNumber, port_number),
        &success));
    success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        shell(), "window.domAutomationController.send(clickCrossSiteLink());",
        &success));
    EXPECT_TRUE(success);
    EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // The RenderFrameHost should not have changed unless site-per-process is
  // enabled.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_NE(initial_rfh,
              static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetFrameTree()
                  ->root()
                  ->current_frame_host());
  } else {
    EXPECT_EQ(initial_rfh,
              static_cast<WebContentsImpl*>(shell()->web_contents())
                  ->GetFrameTree()
                  ->root()
                  ->current_frame_host());
  }
}

// Ensure that browser side navigation handles navigation failures.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest, FailedNavigation) {
  // Perform a navigation with no live renderer.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL url(embedded_test_server()->GetURL("/title1.html"));
    NavigateToURL(shell(), url);
    EXPECT_EQ(url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  // Now navigate to an unreachable url.
  {
    TestNavigationObserver observer(shell()->web_contents());
    GURL error_url(
        net::URLRequestFailedJob::GetMockHttpUrl(net::ERR_CONNECTION_RESET));
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&net::URLRequestFailedJob::AddUrlHandler));
    NavigateToURL(shell(), error_url);
    EXPECT_EQ(error_url, observer.last_navigation_url());
    NavigationEntry* entry =
        shell()->web_contents()->GetController().GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
  }
}

// Ensure that browser side navigation can load browser initiated navigations
// to view-source URLs.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       ViewSourceNavigation_BrowserInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL url(embedded_test_server()->GetURL("/title1.html"));
  GURL view_source_url(content::kViewSourceScheme + std::string(":") +
                       url.spec());
  NavigateToURL(shell(), view_source_url);
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
}

// Ensure that browser side navigation blocks content initiated navigations to
// view-source URLs.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       ViewSourceNavigation_RendererInitiated) {
  TestNavigationObserver observer(shell()->web_contents());
  GURL kUrl(embedded_test_server()->GetURL("/simple_links.html"));
  NavigateToURL(shell(), kUrl);
  EXPECT_EQ(kUrl, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Not allowed to load local resource: view-source:about:blank"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell()->web_contents(),
      "window.domAutomationController.send(clickViewSourceLink());", &success));
  EXPECT_TRUE(success);
  console_delegate->Wait();
  // Original page shouldn't navigate away.
  EXPECT_EQ(kUrl, shell()->web_contents()->GetURL());
  EXPECT_FALSE(shell()
                   ->web_contents()
                   ->GetController()
                   .GetLastCommittedEntry()
                   ->IsViewSourceMode());
}

// Ensure that closing a page by running its beforeunload handler doesn't hang
// if there's an ongoing navigation.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       UnloadDuringNavigation) {
  content::WindowedNotificationObserver close_observer(
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<content::WebContents>(shell()->web_contents()));
  GURL url("chrome://resources/css/tabs.css");
  NavigationHandleObserver handle_observer(shell()->web_contents(), url);
  shell()->LoadURL(url);
  shell()->web_contents()->DispatchBeforeUnload();
  close_observer.Wait();
  EXPECT_EQ(net::ERR_ABORTED, handle_observer.net_error_code());
}

// Ensure that the referrer of a navigation is properly sanitized.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest, SanitizeReferrer) {
  const GURL kInsecureUrl(embedded_test_server()->GetURL("/title1.html"));
  const Referrer kSecureReferrer(
      GURL("https://secure-url.com"),
      blink::kWebReferrerPolicyNoReferrerWhenDowngrade);
  ShellNetworkDelegate::SetCancelURLRequestWithPolicyViolatingReferrerHeader(
      true);

  // Navigate to an insecure url with a secure referrer with a policy of no
  // referrer on downgrades. The referrer url should be rewritten right away.
  NavigationController::LoadURLParams load_params(kInsecureUrl);
  load_params.referrer = kSecureReferrer;
  TestNavigationManager manager(shell()->web_contents(), kInsecureUrl);
  shell()->web_contents()->GetController().LoadURLWithParams(load_params);
  EXPECT_TRUE(manager.WaitForRequestStart());

  // The referrer should have been sanitized.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetMainFrame()
                            ->frame_tree_node();
  ASSERT_TRUE(root->navigation_request());
  EXPECT_EQ(GURL(),
            root->navigation_request()->navigation_handle()->GetReferrer().url);

  // The navigation should commit without being blocked.
  EXPECT_TRUE(manager.WaitForResponse());
  manager.WaitForNavigationFinished();
  EXPECT_EQ(kInsecureUrl, shell()->web_contents()->GetLastCommittedURL());
}

// Test to verify that an exploited renderer process trying to upload a file
// it hasn't been explicitly granted permissions to is correctly terminated.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       PostUploadIllegalFilePath) {
  GURL form_url(
      embedded_test_server()->GetURL("/form_that_posts_to_echoall.html"));
  EXPECT_TRUE(NavigateToURL(shell(), form_url));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  // Prepare a file for the upload form.
  base::ThreadRestrictions::ScopedAllowIO allow_io_for_temp_dir;
  base::ScopedTempDir temp_dir;
  base::FilePath file_path;
  std::string file_content("test-file-content");
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &file_path));
  ASSERT_LT(
      0, base::WriteFile(file_path, file_content.data(), file_content.size()));

  // Fill out the form to refer to the test file.
  std::unique_ptr<FileChooserDelegate> delegate(
      new FileChooserDelegate(file_path));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.getElementById('file').click();"));
  EXPECT_TRUE(delegate->file_chosen());

  // Ensure that the process is allowed to access to the chosen file and
  // does not have access to the other file name.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      rfh->GetProcess()->GetID(), file_path));

  // Revoke the access to the file and submit the form. The renderer process
  // should be terminated.
  RenderProcessHostWatcher process_exit_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  ChildProcessSecurityPolicyImpl* security_policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  security_policy->RevokeAllPermissionsForFile(rfh->GetProcess()->GetID(),
                                               file_path);

  // Use ExecuteScriptAndExtractBool and respond back to the browser process
  // before doing the actual submission. This will ensure that the process
  // termination is guaranteed to arrive after the response from the executed
  // JavaScript.
  bool result = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(true);"
      "document.getElementById('file-form').submit();",
      &result));
  EXPECT_TRUE(result);
  process_exit_observer.Wait();
}

// Test case to verify that redirects to data: URLs are properly disallowed,
// even when invoked through a reload.
// See https://crbug.com/723796.
//
// Note: This is PlzNavigate specific test, as the behavior of reloads in the
// non-PlzNavigate path differs. The WebURLRequest for the reload is generated
// based on Blink's state instead of the history state in the browser process,
// which ends up loading the originally blocked URL. With PlzNavigate, the
// reload uses the NavigationEntry state to create a navigation and commit it.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest,
                       VerifyBlockedErrorPageURL_Reload) {
  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());

  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));
  EXPECT_EQ(0, controller.GetLastCommittedEntryIndex());

  // Navigate to an URL, which redirects to a data: URL, since it is an
  // unsafe redirect and will result in a blocked navigation and error page.
  GURL redirect_to_blank_url(
      embedded_test_server()->GetURL("/server-redirect?data:text/html,Hello!"));
  EXPECT_FALSE(NavigateToURL(shell(), redirect_to_blank_url));
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_EQ(PAGE_TYPE_ERROR, controller.GetLastCommittedEntry()->GetPageType());

  TestNavigationObserver reload_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell(), "location.reload()"));
  reload_observer.Wait();

  // The expectation is that about:blank was loaded and the virtual URL is set
  // to the URL that was blocked.
  //
  // TODO(nasko): Now that the error commits on the previous URL, the blocked
  // navigation logic is no longer needed. https://crbug.com/723796
  EXPECT_EQ(1, controller.GetLastCommittedEntryIndex());
  EXPECT_FALSE(
      controller.GetLastCommittedEntry()->GetURL().SchemeIs(url::kDataScheme));
  EXPECT_EQ(redirect_to_blank_url,
            controller.GetLastCommittedEntry()->GetVirtualURL());
  EXPECT_EQ(url::kAboutBlankURL,
            controller.GetLastCommittedEntry()->GetURL().spec());
}

class BrowserSideNavigationBrowserDisableWebSecurityTest
    : public BrowserSideNavigationBrowserTest {
 public:
  BrowserSideNavigationBrowserDisableWebSecurityTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Simulate a compromised renderer, otherwise the cross-origin request to
    // file: is blocked.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
    BrowserSideNavigationBrowserTest::SetUpCommandLine(command_line);
  }
};

// Test to verify that an exploited renderer process trying to specify a
// non-empty URL for base_url_for_data_url on navigation is correctly
// terminated.
// TODO(nasko): This test case belongs better in
// security_exploit_browsertest.cc, so move it there once PlzNavigate is on
// by default.
IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserDisableWebSecurityTest,
                       ValidateBaseUrlForDataUrl) {
  GURL start_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());

  GURL data_url("data:text/html,foo");
  base::FilePath file_path = GetTestFilePath("", "simple_page.html");
  GURL file_url = net::FilePathToFileURL(file_path);

  // To get around DataUrlNavigationThrottle. Other attempts at getting around
  // it don't work, i.e.:
  // -if the request is made in a child frame then the frame is torn down
  // immediately on process killing so the navigation doesn't complete
  // -if it's classified as same document, then a DCHECK in
  // NavigationRequest::CreateRendererInitiated fires
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAllowContentInitiatedDataUrlNavigations);
  // Setup a BeginNavigate IPC with non-empty base_url_for_data_url.
  CommonNavigationParams common_params(
      data_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT, true, false,
      base::TimeTicks(), FrameMsg_UILoadMetricsReportType::NO_REPORT,
      file_url,  // base_url_for_data_url
      GURL(), PREVIEWS_UNSPECIFIED, base::TimeTicks::Now(), "GET", nullptr,
      base::Optional<SourceLocation>(), CSPDisposition::CHECK);
  BeginNavigationParams begin_params(
      std::string(), net::LOAD_NORMAL, false, false,
      REQUEST_CONTEXT_TYPE_LOCATION,
      blink::WebMixedContentContextType::kBlockable, false,
      url::Origin::Create(data_url));
  FrameHostMsg_BeginNavigation msg(rfh->GetRoutingID(), common_params,
                                   begin_params);

  // Receiving the invalid IPC message should lead to renderer process
  // termination.
  RenderProcessHostWatcher process_exit_observer(
      rfh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  IPC::IpcSecurityTestUtil::PwnMessageReceived(rfh->GetProcess()->GetChannel(),
                                               msg);
  process_exit_observer.Wait();

  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      rfh->GetProcess()->GetID(), file_path));

  // Reload the page to create another renderer process.
  TestNavigationObserver tab_observer(shell()->web_contents(), 1);
  shell()->web_contents()->GetController().Reload(ReloadType::NORMAL, false);
  tab_observer.Wait();

  // Make an XHR request to check if the page has access.
  std::string script = base::StringPrintf(
      "var xhr = new XMLHttpRequest()\n"
      "xhr.open('GET', '%s', false);\n"
      "xhr.send();\n"
      "window.domAutomationController.send(xhr.responseText);",
      file_url.spec().c_str());
  std::string result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(shell()->web_contents(), script, &result));
  EXPECT_TRUE(result.empty());
}

IN_PROC_BROWSER_TEST_F(BrowserSideNavigationBrowserTest, BackFollowedByReload) {
  // First, make two history entries.
  GURL url1(embedded_test_server()->GetURL("/title1.html"));
  GURL url2(embedded_test_server()->GetURL("/title2.html"));
  NavigateToURL(shell(), url1);
  NavigateToURL(shell(), url2);

  // Then execute a back navigation in Javascript followed by a reload.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "history.back(); location.reload();"));
  navigation_observer.Wait();

  // The reload should have cancelled the back navigation, and the last
  // committed URL should still be the second URL.
  EXPECT_EQ(url2, shell()->web_contents()->GetLastCommittedURL());
}

}  // namespace content
