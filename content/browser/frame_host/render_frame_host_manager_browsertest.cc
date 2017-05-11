// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/input_messages.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/page_state.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/web_preferences.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

using base::ASCIIToUTF16;

namespace content {

namespace {

const char kOpenUrlViaClickTargetFunc[] =
    "(function(url) {\n"
    "  var lnk = document.createElement(\"a\");\n"
    "  lnk.href = url;\n"
    "  lnk.target = \"_blank\";\n"
    "  document.body.appendChild(lnk);\n"
    "  lnk.click();\n"
    "})";

// Adds a link with given url and target=_blank, and clicks on it.
void OpenUrlViaClickTarget(const ToRenderFrameHost& adapter, const GURL& url) {
  EXPECT_TRUE(ExecuteScript(adapter,
      std::string(kOpenUrlViaClickTargetFunc) + "(\"" + url.spec() + "\");"));
}

class TestWebUIMessageHandler : public WebUIMessageHandler {
 public:
  using WebUIMessageHandler::AllowJavascript;
  using WebUIMessageHandler::IsJavascriptAllowed;

 protected:
  void RegisterMessages() override {}
};

// This class implements waiting for RenderFrameHost destruction. It relies on
// the fact that RenderFrameDeleted event is fired when RenderFrameHost is
// destroyed.
// Note: RenderFrameDeleted is also fired when the process associated with the
// RenderFrameHost crashes, so this cannot be used in cases where process dying
// is expected.
class RenderFrameHostDestructionObserver : public WebContentsObserver {
 public:
  explicit RenderFrameHostDestructionObserver(RenderFrameHost* rfh)
      : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
        message_loop_runner_(new MessageLoopRunner),
        deleted_(false),
        render_frame_host_(rfh) {}
  ~RenderFrameHostDestructionObserver() override {}

  bool deleted() const { return deleted_; }

  void Wait() {
    if (deleted_)
      return;

    message_loop_runner_->Run();
  }

  // WebContentsObserver implementation:
  void RenderFrameDeleted(RenderFrameHost* rfh) override {
    if (rfh == render_frame_host_) {
      CHECK(!deleted_);
      deleted_ = true;
    }

    if (deleted_ && message_loop_runner_->loop_running()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, message_loop_runner_->QuitClosure());
    }
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  bool deleted_;
  RenderFrameHost* render_frame_host_;
};

}  // anonymous namespace

class RenderFrameHostManagerTest : public ContentBrowserTest {
 public:
  RenderFrameHostManagerTest() : foo_com_("foo.com") {
    replace_host_.SetHostStr(foo_com_);
  }

  static void GetFilePathWithHostAndPortReplacement(
      const std::string& original_file_path,
      const net::HostPortPair& host_port_pair,
      std::string* replacement_path) {
    base::StringPairs replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_HOST_AND_PORT", host_port_pair.ToString()));
    net::test_server::GetFilePathWithReplacements(
        original_file_path, replacement_text, replacement_path);
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void StartServer() {
    ASSERT_TRUE(embedded_test_server()->Start());

    foo_host_port_ = embedded_test_server()->host_port_pair();
    foo_host_port_.set_host(foo_com_);
  }

  void StartEmbeddedServer() {
    SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  // Returns a URL on foo.com with the given path.
  GURL GetCrossSiteURL(const std::string& path) {
    GURL cross_site_url(embedded_test_server()->GetURL(path));
    return cross_site_url.ReplaceComponents(replace_host_);
  }

  void NavigateToPageWithLinks(Shell* shell) {
    EXPECT_TRUE(NavigateToURL(
        shell, embedded_test_server()->GetURL("/click-noreferrer-links.html")));

    // Rewrite selected links on the page to be actual cross-site (bar.com)
    // URLs. This does not use the /cross-site/ redirector, since that creates
    // links that initially look same-site.
    std::string script = "setOriginForLinks('http://bar.com:" +
                         embedded_test_server()->base_url().port() + "/');";
    EXPECT_TRUE(ExecuteScript(shell, script));
  }

 protected:
  std::string foo_com_;
  GURL::Replacements replace_host_;
  net::HostPortPair foo_host_port_;
};

// Web pages should not have script access to the swapped out page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, NoScriptAccessAfterSwapOut) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Open a same-site link in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);

  // Now navigate the new window to a different site.
  NavigateToURL(new_shell,
                embedded_test_server()->GetURL("foo.com", "/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // We should no longer have script access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_FALSE(success);

  // We now navigate the window to an about:blank page.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickBlankTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish.
  WaitForLoadStop(new_shell->web_contents());
  GURL blank_url(url::kAboutBlankURL);
  EXPECT_EQ(blank_url,
            new_shell->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(orig_site_instance, new_shell->web_contents()->GetSiteInstance());

  // We should have access to the opened window's location.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(testScriptAccessToWindow());",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and target=_blank should create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithRelNoreferrerAndTargetBlank) {
  StartEmbeddedServer();

  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickNoRefTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Same as above, but for 'noopener'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithRelNoopenerAndTargetBlank) {
  StartEmbeddedServer();

  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer + target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickNoOpenerTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_FALSE(
      web_contents->GetRenderManagerForTesting()->pending_render_view_host());

  // Check that the referrer is set correctly.
  std::string expected_referrer =
      embedded_test_server()->GetURL("/click-noreferrer-links.html").spec();
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(document.referrer == '" +
                     expected_referrer + "');",
      &success));
  EXPECT_TRUE(success);

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noopener_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noopener_blank_site_instance);
}

// 'noopener' also works from 'window.open'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithWindowOpenAndNoopener) {
  StartEmbeddedServer();

  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get());

  // Test opening a window with the 'noopener' feature.
  ShellAddedObserver new_shell_observer;
  bool hasWindowReference = true;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send("
      "    openWindowWithTargetAndFeatures('/title2.html', '', 'noopener')"
      ");",
      &hasWindowReference));
  // We should not get a reference to the opened window.
  EXPECT_FALSE(hasWindowReference);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_FALSE(
      web_contents->GetRenderManagerForTesting()->pending_render_view_host());

  EXPECT_EQ("/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Check that `window.opener` is not set.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Check that the referrer is set correctly.
  std::string expected_referrer =
      embedded_test_server()->GetURL("/click-noreferrer-links.html").spec();
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(document.referrer == '" +
                     expected_referrer + "');",
      &success));
  EXPECT_TRUE(success);

  // Should have a new SiteInstance.
  scoped_refptr<SiteInstance> noopener_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noopener_blank_site_instance);
}

// As of crbug.com/69267, we create a new BrowsingInstance (and SiteInstance)
// for rel=noreferrer links in new windows, even to same site pages and named
// targets.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithSameSiteRelNoreferrer) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a same-site rel=noreferrer + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteNoRefTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  EXPECT_FALSE(
      web_contents->GetRenderManagerForTesting()->pending_render_view_host());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Same as above, but for 'noopener'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SwapProcessWithSameSiteRelNoopener) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a same-site rel=noopener + target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(),
                                          "window.domAutomationController.send("
                                          "clickSameSiteNoOpenerTargetedLink())"
                                          ";",
                                          &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Opens in new window.
  EXPECT_EQ("/title2.html", new_shell->web_contents()->GetVisibleURL().path());

  // Check that `window.opener` is not set.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the new tab to finish.
  WaitForLoadStop(new_shell->web_contents());
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  EXPECT_FALSE(web_contents->GetRenderManagerForTesting()->
      pending_render_view_host());

  // Should have a new SiteInstance (in a new BrowsingInstance).
  scoped_refptr<SiteInstance> noref_blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, noref_blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with just
// target=_blank should not create a new SiteInstance, unless we
// are running in --site-per-process mode.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyTargetBlank) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the cross-site transition in the new tab to finish.
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
  EXPECT_EQ("/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  if (AreAllSitesIsolatedForTesting())
    EXPECT_NE(orig_site_instance, blank_site_instance);
  else
    EXPECT_EQ(orig_site_instance, blank_site_instance);
}

// Test for crbug.com/24447.  Following a cross-site link with rel=noreferrer
// and no target=_blank should not create a new SiteInstance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyRelNoreferrer) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  if (AreAllSitesIsolatedForTesting())
    EXPECT_NE(orig_site_instance, noref_site_instance);
  else
    EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Same as above, but for 'noopener'
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontSwapProcessWithOnlyRelNoOpener) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a rel=noreferrer link.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(clickNoRefLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the cross-site transition in the current tab to finish.
  WaitForLoadStop(shell()->web_contents());

  // Opens in same window.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance unless we're in site-per-process mode.
  scoped_refptr<SiteInstance> noref_site_instance(
      shell()->web_contents()->GetSiteInstance());
  if (AreAllSitesIsolatedForTesting())
    EXPECT_NE(orig_site_instance, noref_site_instance);
  else
    EXPECT_EQ(orig_site_instance, noref_site_instance);
}

// Test for crbug.com/116192.  Targeted links should still work after the
// named target window has swapped processes.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       AllowTargetedNavigationsAfterSwap) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  NavigateToURL(new_shell, cross_site_url);
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Clicking the original link in the first tab should cause us to swap back.
  TestNavigationObserver navigation_observer(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back and shown the new window again.
  scoped_refptr<SiteInstance> revisit_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);

  // If it navigates away to another process, the original window should
  // still be able to close it (using a cross-process close message).
  NavigateToURL(new_shell, cross_site_url);
  EXPECT_EQ(new_site_instance.get(),
            new_shell->web_contents()->GetSiteInstance());
  WebContentsDestroyedWatcher close_watcher(new_shell->web_contents());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(testCloseWindow());",
      &success));
  EXPECT_TRUE(success);
  close_watcher.Wait();
}

// Test that setting the opener to null in a window affects cross-process
// navigations, including those to existing entries.  http://crbug.com/156669.
// This test crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_DisownOpener DISABLED_DisownOpener
#else
#define MAYBE_DisownOpener DisownOpener
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, MAYBE_DisownOpener) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=_blank link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/title2.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the new tab to a different site.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  NavigateToURL(new_shell, cross_site_url);
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Now disown the opener.
  EXPECT_TRUE(ExecuteScript(new_shell, "window.opener = null;"));
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());

  // Go back and ensure the opener is still null.
  {
    TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
    new_shell->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());

  // Now navigate forward again (creating a new process) and check opener.
  NavigateToURL(new_shell, cross_site_url);
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(window.opener == null);",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(new_shell->web_contents()->HasOpener());
}

// Test that subframes can disown their openers.  http://crbug.com/225528.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, DisownSubframeOpener) {
  const GURL frame_url("data:text/html,<iframe name=\"foo\"></iframe>");
  NavigateToURL(shell(), frame_url);

  // Give the frame an opener using window.open.
  EXPECT_TRUE(ExecuteScript(shell(), "window.open('about:blank','foo');"));

  // Check that the browser process updates the subframe's opener.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  EXPECT_EQ(root, root->child_at(0)->opener());
  EXPECT_EQ(nullptr, root->child_at(0)->original_opener());

  // Now disown the frame's opener.  Shouldn't crash.
  EXPECT_TRUE(ExecuteScript(shell(), "window.frames[0].opener = null;"));

  // Check that the subframe's opener in the browser process is disowned.
  EXPECT_EQ(nullptr, root->child_at(0)->opener());
  EXPECT_EQ(nullptr, root->child_at(0)->original_opener());
}

// Check that window.name is preserved for top frames when they navigate
// cross-process.  See https://crbug.com/504164.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PreserveTopFrameWindowNameOnCrossProcessNavigations) {
  StartEmbeddedServer();

  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Open a popup using window.open with a 'foo' window.name.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(new_shell);

  // The window.name for the new popup should be "foo".
  std::string name;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell, "window.domAutomationController.send(window.name);", &name));
  EXPECT_EQ("foo", name);

  // Now navigate the new tab to a different site.
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(new_shell, foo_url));
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // window.name should still be "foo".
  name = "";
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_shell, "window.domAutomationController.send(window.name);", &name));
  EXPECT_EQ("foo", name);

  // Open another popup from the 'foo' popup and navigate it cross-site.
  Shell* new_shell2 = OpenPopup(new_shell, GURL(url::kAboutBlankURL), "bar");
  EXPECT_TRUE(new_shell2);
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURL(new_shell2, bar_url));

  // Check that the new popup's window.opener has name "foo", which verifies
  // that new swapped-out RenderViews also propagate window.name.  This has to
  // be done via window.open, since window.name isn't readable cross-origin.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell2,
      "window.domAutomationController.send("
      "    window.opener === window.open('','foo'));",
      &success));
  EXPECT_TRUE(success);
}

// Test for crbug.com/99202.  PostMessage calls should still work after
// navigating the source and target windows to different sites.
// Specifically:
// 1) Create 3 windows (opener, "foo", and _blank) and send "foo" cross-process.
// 2) Fail to post a message from "foo" to opener with the wrong target origin.
// 3) Post a message from "foo" to opener, which replies back to "foo".
// 4) Post a message from _blank to "foo".
// 5) Post a message from "foo" to a subframe of opener, which replies back.
// 6) Post a message from _blank to a subframe of "foo".
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SupportCrossProcessPostMessage) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);
  RenderFrameHostManager* opener_manager = static_cast<WebContentsImpl*>(
      opener_contents)->GetRenderManagerForTesting();

  // 1) Open two more windows, one named.  These initially have openers but no
  // reference to each other.  We will later post a message between them.

  // First, a named target=foo window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell, embedded_test_server()->GetURL(
                               "foo.com", "/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // Second, a target=_blank window.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetBlankLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on the original site.
  Shell* new_shell2 = new_shell_observer2.GetShell();
  WebContents* new_contents = new_shell2->web_contents();
  WaitForLoadStop(new_contents);
  EXPECT_EQ("/title2.html", new_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell2,
                embedded_test_server()->GetURL("/post_message.html"));
  EXPECT_EQ(orig_site_instance.get(), new_contents->GetSiteInstance());
  RenderFrameHostManager* new_manager =
      static_cast<WebContentsImpl*>(new_contents)->GetRenderManagerForTesting();

  // We now have three windows.  The opener should have a swapped out RVH
  // for the new SiteInstance, but the _blank window should not.
  EXPECT_EQ(3u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));
  EXPECT_FALSE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Fail to post a message from the foo window to the opener if the target
  // origin is wrong.  We won't see an error, but we can check for the right
  // number of received messages below.
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg',"
      "    'http://google.com'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));

  // 3) Post a message from the foo window to the opener.  The opener will
  // reply, causing the foo window to update its own title.
  base::string16 expected_title = ASCIIToUTF16("msg");
  TitleWatcher title_watcher(foo_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_contents,
      "window.domAutomationController.send(postToOpener('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // We should have received only 1 message in the opener and "foo" tabs,
  // and updated the title.
  int opener_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &opener_received_messages));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, opener_received_messages);
  EXPECT_EQ(ASCIIToUTF16("msg"), foo_contents->GetTitle());

  // 4) Now post a message from the _blank window to the foo window.  The
  // foo window will update its title and will not reply.
  expected_title = ASCIIToUTF16("msg2");
  TitleWatcher title_watcher2(foo_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_contents,
      "window.domAutomationController.send(postToFoo('msg2'));",
      &success));
  EXPECT_TRUE(success);
  ASSERT_EQ(expected_title, title_watcher2.WaitAndGetTitle());

  // This postMessage should have created a swapped out RVH for the new
  // SiteInstance in the target=_blank window.
  EXPECT_TRUE(
      new_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // TODO(nasko): Test subframe targeting of postMessage once
  // http://crbug.com/153701 is fixed.
}

// Test for crbug.com/278336. MessagePorts should work cross-process. Messages
// which contain Transferables that need to be forwarded between processes via
// RenderFrameProxy::willCheckAndDispatchMessageEvent should work.
// Specifically:
// 1) Create 2 windows (opener and "foo") and send "foo" cross-process.
// 2) Post a message containing a message port from opener to "foo".
// 3) Post a message from "foo" back to opener via the passed message port.
// The test will be enabled when the feature implementation lands.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SupportCrossProcessPostMessageWithMessagePort) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance and RVHM for later comparison.
  WebContents* opener_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      opener_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);
  RenderFrameHostManager* opener_manager = static_cast<WebContentsImpl*>(
      opener_contents)->GetRenderManagerForTesting();

  // 1) Open a named target=foo window. We will later post a message between the
  // opener and the new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't, then
  // send it to post_message.html on a different site.
  WebContents* foo_contents = new_shell->web_contents();
  WaitForLoadStop(foo_contents);
  EXPECT_EQ("/navigate_opener.html",
            foo_contents->GetLastCommittedURL().path());
  NavigateToURL(new_shell, embedded_test_server()->GetURL(
                               "foo.com", "/post_message.html"));
  scoped_refptr<SiteInstance> foo_site_instance(
      foo_contents->GetSiteInstance());
  EXPECT_NE(orig_site_instance, foo_site_instance);

  // We now have two windows. The opener should have a swapped out RVH
  // for the new SiteInstance.
  EXPECT_EQ(2u, Shell::windows().size());
  EXPECT_TRUE(
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get()));

  // 2) Post a message containing a MessagePort from opener to the the foo
  // window. The foo window will reply via the passed port, causing the opener
  // to update its own title.
  base::string16 expected_title = ASCIIToUTF16("msg-back-via-port");
  TitleWatcher title_observer(opener_contents, expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      opener_contents,
      "window.domAutomationController.send(postWithPortToFoo());",
      &success));
  EXPECT_TRUE(success);
  ASSERT_FALSE(
      opener_manager->GetSwappedOutRenderViewHost(orig_site_instance.get()));
  ASSERT_EQ(expected_title, title_observer.WaitAndGetTitle());

  // Check message counts.
  int opener_received_messages_via_port = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      opener_contents,
      "window.domAutomationController.send(window.receivedMessagesViaPort);",
      &opener_received_messages_via_port));
  int foo_received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessages);",
      &foo_received_messages));
  int foo_received_messages_with_port = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      foo_contents,
      "window.domAutomationController.send(window.receivedMessagesWithPort);",
      &foo_received_messages_with_port));
  EXPECT_EQ(1, foo_received_messages);
  EXPECT_EQ(1, foo_received_messages_with_port);
  EXPECT_EQ(1, opener_received_messages_via_port);
  EXPECT_EQ(ASCIIToUTF16("msg-with-port"), foo_contents->GetTitle());
  EXPECT_EQ(ASCIIToUTF16("msg-back-via-port"), opener_contents->GetTitle());
}

// Test for crbug.com/116192.  Navigations to a window's opener should
// still work after a process swap.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       AllowTargetedNavigationsInOpenerAfterSwap) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original tab and SiteInstance for later comparison.
  WebContents* orig_contents = shell()->web_contents();
  scoped_refptr<SiteInstance> orig_site_instance(
      orig_contents->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> blank_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, blank_site_instance);

  // Now navigate the original (opener) tab to a different site.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("foo.com", "/title1.html"));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The opened tab should be able to navigate the opener back to its process.
  TestNavigationObserver navigation_observer(orig_contents);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      new_shell, "window.domAutomationController.send(navigateOpener());",
      &success));
  EXPECT_TRUE(success);
  navigation_observer.Wait();

  // Should have swapped back into this process.
  scoped_refptr<SiteInstance> revisit_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, revisit_site_instance);
}

// Test that subframes do not crash when sending a postMessage to the top frame
// from an unload handler while the top frame is being swapped out as part of
// navigating cross-process.  https://crbug.com/475651.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PostMessageFromSubframeUnloadHandler) {
  StartEmbeddedServer();

  GURL frame_url(embedded_test_server()->GetURL("/post_message.html"));
  GURL main_url("data:text/html,<iframe name='foo' src='" + frame_url.spec() +
                "'></iframe>");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(nullptr, orig_site_instance.get());

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());
  EXPECT_EQ(frame_url, root->child_at(0)->current_url());

  // Register an unload handler that sends a postMessage to the top frame.
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "registerUnload();"));

  // Navigate the top frame cross-site.  This will cause the top frame to be
  // swapped out and run unload handlers, and the original renderer process
  // should then terminate since it's not rendering any other frames.
  RenderProcessHostWatcher exit_observer(
      root->current_frame_host()->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("foo.com", "/title1.html")));
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // Ensure that the original renderer process exited cleanly without crashing.
  exit_observer.Wait();
  EXPECT_EQ(true, exit_observer.did_exit_normally());
}

// Test that opening a new window in the same SiteInstance and then navigating
// both windows to a different SiteInstance allows the first process to exit.
// See http://crbug.com/126333.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ProcessExitWithSwappedOutViews) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> opened_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, opened_site_instance);

  // Now navigate the opened window to a different site.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  NavigateToURL(new_shell, cross_site_url);
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // The original process should still be alive, since it is still used in the
  // first window.
  RenderProcessHost* orig_process = orig_site_instance->GetProcess();
  EXPECT_TRUE(orig_process->HasConnection());

  // Navigate the first window to a different site as well.  The original
  // process should exit, since all of its views are now swapped out.
  RenderProcessHostWatcher exit_observer(
      orig_process,
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), cross_site_url);
  exit_observer.Wait();
  scoped_refptr<SiteInstance> new_site_instance2(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(new_site_instance, new_site_instance2);
}

// Test for crbug.com/76666.  A cross-site navigation that fails with a 204
// error should not make us ignore future renderer-initiated navigations.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, ClickLinkAfter204Error) {
  StartServer();

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance.get() != NULL);

  // Load a cross-site page that fails with a 204 error.
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GetCrossSiteURL("/nocontent")));

  // We should still be looking at the normal page.  Because we started from a
  // blank new tab, the typed URL will still be visible until the user clears it
  // manually.  The last committed URL will be the previous page.
  scoped_refptr<SiteInstance> post_nav_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, post_nav_site_instance);
  EXPECT_EQ("/nocontent",
            shell()->web_contents()->GetVisibleURL().path());
  EXPECT_FALSE(
      shell()->web_contents()->GetController().GetLastCommittedEntry());

  // Renderer-initiated navigations should work.
  base::string16 expected_title = ASCIIToUTF16("Title Of Awesomeness");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  GURL url = embedded_test_server()->GetURL("/title2.html");
  EXPECT_TRUE(ExecuteScript(
      shell(), base::StringPrintf("location.href = '%s'", url.spec().c_str())));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Opens in same tab.
  EXPECT_EQ(1u, Shell::windows().size());
  EXPECT_EQ("/title2.html",
            shell()->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  scoped_refptr<SiteInstance> new_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_EQ(orig_site_instance, new_site_instance);
}

// A collection of tests to prevent URL spoofs when showing pending URLs above
// initial empty documents, ensuring that the URL reverts to about:blank if the
// document is accessed. See https://crbug.com/9682.
class RenderFrameHostManagerSpoofingTest : public RenderFrameHostManagerTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    base::CommandLine new_command_line(command_line->GetProgram());
    base::CommandLine::SwitchMap switches = command_line->GetSwitches();

    // Injecting the DOM automation controller causes false positives, since it
    // triggers the DidAccessInitialDocument() callback by mutating the global
    // object.
    switches.erase(switches::kDomAutomationController);

    for (const auto& it : switches)
      new_command_line.AppendSwitchNative(it.first, it.second);

    *command_line = new_command_line;
  }

 protected:
  // Custom ExecuteScript() helper that doesn't depend on DOM automation
  // controller. This is used to guarantee the script has completed execution,
  // but the spoofing tests synchronize execution using window title changes.
  void ExecuteScript(const ToRenderFrameHost& adapter, const char* script) {
    adapter.render_frame_host()->ExecuteJavaScriptForTests(
        base::UTF8ToUTF16(script));
  }
};

// Helper to wait until a WebContent's NavigationController has a visible entry.
class VisibleEntryWaiter : public WebContentsObserver {
 public:
  explicit VisibleEntryWaiter(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  void Wait() {
    if (web_contents()->GetController().GetVisibleEntry())
      return;
    run_loop_.Run();
  }

  // WebContentsObserver overrides:
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

// Sanity test that a newly opened window shows the pending URL if the initial
// empty document is not modified. This is intentionally structured as similarly
// as possible to the subsequent ShowLoadingURLUntil*Spoof tests: it performs
// the same operations as the subsequent tests except DOM modification. This
// should help catch instances where the subsequent tests incorrectly pass due
// to a side effect of the test infrastructure.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLIfNotModified) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the provisional load, so the
  // associated navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now get another reference to the window object, but don't otherwise access
  // it. This is to ensure that DidAccessInitialDocument() notifications are not
  // incorrectly generated when nothing is modified.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "getNewWindowReference();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // The destination URL should still be visible, since nothing was modified.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());
}

// Test for crbug.com/9682.  We should show the URL for a pending renderer-
// initiated navigation in a new tab, until the content of the initial
// about:blank page is modified by another window.  At that point, we should
// revert to showing about:blank to prevent a URL spoof.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLUntilSpoof) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the provisional load, so the
  // associated navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "modifyNewWindow();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Similar but using document.open(): once a Document is opened, subsequent
// document.write() calls can insert arbitrary content into the target Document.
// Since this could result in URL spoofing, the pending URL should no longer be
// shown in the omnibox.
//
// Note: document.write() implicitly invokes document.open() if the Document has
// not already been opened, so there's no need to test document.write()
// separately.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerSpoofingTest,
                       ShowLoadingURLUntilDocumentOpenSpoof) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  ShellAddedObserver new_shell_observer;
  ExecuteScript(orig_contents, "clickNoContentTargetedLink();");

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();
  WebContents* contents = new_shell->web_contents();

  // Make sure the new window has started the provisional load, so the
  // associated navigation controller will have a visible entry.
  {
    VisibleEntryWaiter waiter(contents);
    waiter.Wait();
  }

  // Ensure the destination URL is visible, because it is considered the
  // initial navigation.
  EXPECT_TRUE(contents->GetController().IsInitialNavigation());
  EXPECT_EQ("/nocontent",
            contents->GetController().GetVisibleEntry()->GetURL().path());

  // Now modify the contents of the new window from the opener.  This will also
  // modify the title of the document to give us something to listen for.
  base::string16 expected_title = ASCIIToUTF16("Modified Title");
  TitleWatcher title_watcher(orig_contents, expected_title);
  ExecuteScript(orig_contents, "modifyNewWindowWithDocumentOpen();");
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // At this point, we should no longer be showing the destination URL.
  // The visible entry should be null, resulting in about:blank in the address
  // bar.
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Test for crbug.com/9682.  We should not show the URL for a pending renderer-
// initiated navigation in a new tab if it is not the initial navigation.  In
// this case, the renderer will not notify us of a modification, so we cannot
// show the pending URL without allowing a spoof.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontShowLoadingURLIfNotInitialNav) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Load a page that can open a URL that won't commit in a new window.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/click-nocontent-link.html"));
  WebContents* orig_contents = shell()->web_contents();

  // Click a /nocontent link that opens in a new window but never commits.
  // By using an onclick handler that first creates the window, the slow
  // navigation is not considered an initial navigation.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      orig_contents,
      "window.domAutomationController.send("
      "clickNoContentScriptedTargetedLink());",
      &success));
  EXPECT_TRUE(success);

  // Wait for the window to open.
  Shell* new_shell = new_shell_observer.GetShell();

  // Ensure the destination URL is not visible, because it is not the initial
  // navigation.
  WebContents* contents = new_shell->web_contents();
  EXPECT_FALSE(contents->GetController().IsInitialNavigation());
  EXPECT_FALSE(contents->GetController().GetVisibleEntry());
}

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_BackForwardNotStale DISABLED_BackForwardNotStale
#else
#define MAYBE_BackForwardNotStale BackForwardNotStale
#endif
// Test for http://crbug.com/93427.  Ensure that cross-site navigations
// do not cause back/forward navigations to be considered stale by the
// renderer.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, MAYBE_BackForwardNotStale) {
  StartEmbeddedServer();
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

  // Visit a page on first site.
  NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html"));

  // Visit three pages on second site.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("foo.com", "/title1.html"));
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("foo.com", "/title2.html"));
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("foo.com", "/title3.html"));

  // History is now [blank, A1, B1, B2, *B3].
  WebContents* contents = shell()->web_contents();
  EXPECT_EQ(5, contents->GetController().GetEntryCount());

  // Open another window in same process to keep this process alive.
  Shell* new_shell = CreateBrowser();
  NavigateToURL(new_shell,
                embedded_test_server()->GetURL("foo.com", "/title1.html"));

  // Go back three times to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward twice to B2.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoForward();
    forward_nav_load_observer.Wait();
  }

  // Go back twice to first site.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // Now go forward directly to B3.  Shouldn't be left spinning.
  {
    TestNavigationObserver forward_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoToIndex(4);
    forward_nav_load_observer.Wait();
  }
}

// This class ensures that all the given RenderViewHosts have properly been
// shutdown.
class RenderViewHostDestructionObserver : public WebContentsObserver {
 public:
  explicit RenderViewHostDestructionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~RenderViewHostDestructionObserver() override {}
  void EnsureRVHGetsDestructed(RenderViewHost* rvh) {
    watched_render_view_hosts_.insert(rvh);
  }
  size_t GetNumberOfWatchedRenderViewHosts() const {
    return watched_render_view_hosts_.size();
  }

 private:
  // WebContentsObserver implementation:
  void RenderViewDeleted(RenderViewHost* rvh) override {
    watched_render_view_hosts_.erase(rvh);
  }

  std::set<RenderViewHost*> watched_render_view_hosts_;
};

// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(THREAD_SANITIZER)
#define MAYBE_LeakingRenderViewHosts DISABLED_LeakingRenderViewHosts
#else
#define MAYBE_LeakingRenderViewHosts LeakingRenderViewHosts
#endif
// Test for crbug.com/90867. Make sure we don't leak render view hosts since
// they may cause crashes or memory corruptions when trying to call dead
// delegate_. This test also verifies crbug.com/117420 and crbug.com/143255 to
// ensure that a separate SiteInstance is created when navigating to view-source
// URLs, regardless of current URL.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_LeakingRenderViewHosts) {
  StartEmbeddedServer();

  // Observe the created render_view_host's to make sure they will not leak.
  RenderViewHostDestructionObserver rvh_observers(shell()->web_contents());

  GURL navigated_url(embedded_test_server()->GetURL("/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       navigated_url.spec());

  // Let's ensure that when we start with a blank window, navigating away to a
  // view-source URL, we create a new SiteInstance.
  RenderViewHost* blank_rvh = shell()->web_contents()->GetRenderViewHost();
  SiteInstance* blank_site_instance = blank_rvh->GetSiteInstance();
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), GURL::EmptyGURL());
  EXPECT_EQ(blank_site_instance->GetSiteURL(), GURL::EmptyGURL());
  rvh_observers.EnsureRVHGetsDestructed(blank_rvh);

  // Now navigate to the view-source URL and ensure we got a different
  // SiteInstance and RenderViewHost.
  NavigateToURL(shell(), view_source_url);
  EXPECT_NE(blank_rvh, shell()->web_contents()->GetRenderViewHost());
  EXPECT_NE(blank_site_instance, shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance());
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  // Load a random page and then navigate to view-source: of it.
  // This used to cause two RVH instances for the same SiteInstance, which
  // was a problem.  This is no longer the case.
  NavigateToURL(shell(), navigated_url);
  SiteInstance* site_instance1 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  NavigateToURL(shell(), view_source_url);
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());
  SiteInstance* site_instance2 = shell()->web_contents()->
      GetRenderViewHost()->GetSiteInstance();

  // Ensure that view-source navigations force a new SiteInstance.
  EXPECT_NE(site_instance1, site_instance2);

  // Now navigate to a different instance so that we swap out again.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("foo.com", "/title2.html"));
  rvh_observers.EnsureRVHGetsDestructed(
      shell()->web_contents()->GetRenderViewHost());

  // This used to leak a render view host.
  shell()->Close();

  RunAllPendingInMessageLoop();  // Needed on ChromeOS.

  EXPECT_EQ(0U, rvh_observers.GetNumberOfWatchedRenderViewHosts());
}

// Test for crbug.com/143155.  Frame tree updates during unload should not
// interrupt the intended navigation and show swappedout:// instead.
// Specifically:
// 1) Open 2 tabs in an HTTP SiteInstance, with a subframe in the opener.
// 2) Send the second tab to a different foo.com SiteInstance.
//    This creates a swapped out opener for the first tab in the foo process.
// 3) Navigate the first tab to the foo.com SiteInstance, and have the first
//    tab's unload handler remove its frame.
// This used to cause an update to the frame tree of the swapped out RV,
// just as it was navigating to a real page.  That pre-empted the real
// navigation and visibly sent the tab to swappedout://.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       DontPreemptNavigationWithFrameTreeUpdate) {
  StartEmbeddedServer();

  // 1. Load a page that deletes its iframe during unload.
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/remove_frame_on_unload.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  // Open a same-site page in a new window.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(), "window.domAutomationController.send(openWindow());", &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new window to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/title1.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Should have the same SiteInstance.
  EXPECT_EQ(orig_site_instance.get(),
            new_shell->web_contents()->GetSiteInstance());

  // 2. Send the second tab to a different process.
  GURL cross_site_url(
      embedded_test_server()->GetURL("foo.com", "/title1.html"));
  NavigateToURL(new_shell, cross_site_url);
  scoped_refptr<SiteInstance> new_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(orig_site_instance, new_site_instance);

  // 3. Send the first tab to the second tab's process.
  NavigateToURL(shell(), cross_site_url);

  // Make sure it ends up at the right page.
  WaitForLoadStop(shell()->web_contents());
  EXPECT_EQ(cross_site_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ(new_site_instance.get(),
            shell()->web_contents()->GetSiteInstance());
}

// Ensure that renderer-side debug URLs do not cause a process swap, since they
// are meant to run in the current page.  We had a bug where we expected a
// BrowsingInstance swap to occur on pages like view-source and extensions,
// which broke chrome://crash and javascript: URLs.
// See http://crbug.com/335503.
// The test fails on Mac OSX with ASAN.
// See http://crbug.com/699062.
#if defined(OS_MACOSX) && defined(THREAD_SANITIZER)
#define MAYBE_RendererDebugURLsDontSwap DISABLED_RendererDebugURLsDontSwap
#else
#define MAYBE_RendererDebugURLsDontSwap RendererDebugURLsDontSwap
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_RendererDebugURLsDontSwap) {
  StartEmbeddedServer();

  GURL original_url(embedded_test_server()->GetURL("/title2.html"));
  GURL view_source_url(kViewSourceScheme + std::string(":") +
                       original_url.spec());

  NavigateToURL(shell(), view_source_url);

  // Check that javascript: URLs work.
  base::string16 expected_title = ASCIIToUTF16("msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  shell()->LoadURL(GURL("javascript:document.title='msg'"));
  ASSERT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Crash the renderer of the view-source page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();
}

// Ensure that renderer-side debug URLs don't take effect on crashed renderers.
// Otherwise, we might try to load an unprivileged about:blank page into a
// WebUI-enabled RenderProcessHost, failing a safety check in InitRenderView.
// See http://crbug.com/334214.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       IgnoreRendererDebugURLsWhenCrashed) {
  // Visit a WebUI page with bindings.
  GURL webui_url = GURL(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));

  // Crash the renderer of the WebUI page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();

  // Load the crash URL again but don't wait for any action.  If it is not
  // ignored this time, we will fail the WebUI CHECK in InitRenderView.
  shell()->LoadURL(GURL(kChromeUICrashURL));

  // Ensure that such URLs can still work as the initial navigation of a tab.
  // We postpone the initial navigation of the tab using an empty GURL, so that
  // we can add a watcher for crashes.
  Shell* shell2 = Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), GURL(), NULL, gfx::Size());
  RenderProcessHostWatcher crash_observer2(
      shell2->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell2, GURL(kChromeUIKillURL)));
  crash_observer2.Wait();
}

// Ensure that renderer-side debug URLs don't take effect on crashed renderers,
// even when going back/forward.
// See https://crbug.com/477606.

// This test is flaky on Android. crbug.com/585327
#if defined(OS_ANDROID)
#define MAYBE_IgnoreForwardToRendererDebugURLsWhenCrashed \
    DISABLED_IgnoreForwardToRendererDebugURLsWhenCrashed
#else
#define MAYBE_IgnoreForwardToRendererDebugURLsWhenCrashed \
    IgnoreForwardToRendererDebugURLsWhenCrashed
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_IgnoreForwardToRendererDebugURLsWhenCrashed) {
  // Visit a WebUI page with bindings.
  GURL webui_url = GURL(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));

  // Visit a debug URL that manages to commit, then go back.
  NavigateToURL(shell(), GURL(kChromeUIDumpURL));
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();

  // Crash the renderer of the WebUI page.
  RenderProcessHostWatcher crash_observer(
      shell()->web_contents(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(
      NavigateToURLAndExpectNoCommit(shell(), GURL(kChromeUICrashURL)));
  crash_observer.Wait();

  // Going forward with no live renderer should have no effect, and should not
  // crash.
  EXPECT_TRUE(shell()->web_contents()->GetController().CanGoForward());
  shell()->web_contents()->GetController().GoForward();
  EXPECT_FALSE(shell()->web_contents()->GetController().GetPendingEntry());
  EXPECT_TRUE(shell()->web_contents()->GetController().CanGoForward());
}

// The test fails with Android ASAN with changes in v8 that seem unrelated.
//   See http://crbug.com/428329.
#if defined(OS_ANDROID) && defined(THREAD_SANITIZER)
#define MAYBE_ClearPendingWebUIOnCommit DISABLED_ClearPendingWebUIOnCommit
#else
#define MAYBE_ClearPendingWebUIOnCommit ClearPendingWebUIOnCommit
#endif
// Ensure that pending_and_current_web_ui_ is cleared when a URL commits.
// Otherwise it might get picked up by InitRenderView when granting bindings
// to other RenderViewHosts.  See http://crbug.com/330811.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_ClearPendingWebUIOnCommit) {
  // Visit a WebUI page with bindings.
  GURL webui_url(GURL(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost)));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  WebUIImpl* webui = root->current_frame_host()->web_ui();
  EXPECT_TRUE(webui);
  EXPECT_FALSE(
      web_contents->GetRenderManagerForTesting()->GetNavigatingWebUI());

  // Navigate to another WebUI URL that reuses the WebUI object. Make sure we
  // clear GetNavigatingWebUI() when it commits.
  GURL webui_url2(webui_url.spec() + "#foo");
  NavigateToURL(shell(), webui_url2);
  EXPECT_EQ(webui, root->current_frame_host()->web_ui());
  EXPECT_FALSE(
      web_contents->GetRenderManagerForTesting()->GetNavigatingWebUI());
}

class RFHMProcessPerTabTest : public RenderFrameHostManagerTest {
 public:
  RFHMProcessPerTabTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kProcessPerTab);
  }
};

// Test that we still swap processes for BrowsingInstance changes even in
// --process-per-tab mode.  See http://crbug.com/343017.
// Disabled on Android: http://crbug.com/345873.
// Crashes under ThreadSanitizer, http://crbug.com/356758.
#if defined(OS_ANDROID) || defined(THREAD_SANITIZER)
#define MAYBE_BackFromWebUI DISABLED_BackFromWebUI
#else
#define MAYBE_BackFromWebUI BackFromWebUI
#endif
IN_PROC_BROWSER_TEST_F(RFHMProcessPerTabTest, MAYBE_BackFromWebUI) {
  StartEmbeddedServer();
  GURL original_url(embedded_test_server()->GetURL("/title2.html"));
  NavigateToURL(shell(), original_url);

  // Visit a WebUI page with bindings.
  GURL webui_url(GURL(std::string(kChromeUIScheme) + "://" +
                      std::string(kChromeUIGpuHost)));
  NavigateToURL(shell(), webui_url);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));

  // Go back and ensure we have no WebUI bindings.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));
}

// crbug.com/372360
// The test loads url1, opens a link pointing to url2 in a new tab, and
// navigates the new tab to url1.
// The following is needed for the bug to happen:
//  - url1 must require webui bindings;
//  - navigating to url2 in the site instance of url1 should not swap
//   browsing instances, but should require a new site instance.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, WebUIGetsBindings) {
  GURL url1(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIGpuHost));
  GURL url2(std::string(kChromeUIScheme) + "://" +
            std::string(kChromeUIAccessibilityHost));

  // Visit a WebUI page with bindings.
  NavigateToURL(shell(), url1);
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                  shell()->web_contents()->GetRenderProcessHost()->GetID()));
  SiteInstance* site_instance1 = shell()->web_contents()->GetSiteInstance();

  // Open a new tab. Initially it gets a render view in the original tab's
  // current site instance.
  TestNavigationObserver nav_observer(NULL);
  nav_observer.StartWatchingNewWebContents();
  ShellAddedObserver shao;
  OpenUrlViaClickTarget(shell(), url2);
  nav_observer.Wait();
  Shell* new_shell = shao.GetShell();
  WebContentsImpl* new_web_contents = static_cast<WebContentsImpl*>(
      new_shell->web_contents());
  SiteInstance* site_instance2 = new_web_contents->GetSiteInstance();

  EXPECT_NE(site_instance2, site_instance1);
  EXPECT_TRUE(site_instance2->IsRelatedSiteInstance(site_instance1));
  RenderViewHost* initial_rvh = new_web_contents->
      GetRenderManagerForTesting()->GetSwappedOutRenderViewHost(site_instance1);
  ASSERT_TRUE(initial_rvh);

  // Navigate to url1 and check bindings.
  NavigateToURL(new_shell, url1);
  // The navigation should have used the first SiteInstance, otherwise
  // |initial_rvh| did not have a chance to be used.
  EXPECT_EQ(new_web_contents->GetSiteInstance(), site_instance1);
  EXPECT_EQ(BINDINGS_POLICY_WEB_UI,
            new_web_contents->GetMainFrame()->GetEnabledBindings());
}

// crbug.com/424526
// The test loads a WebUI page in process-per-tab mode, then navigates to a
// blank page and then to a regular page. The bug reproduces if blank page is
// visited in between WebUI and regular page.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ForceSwapAfterWebUIBindings) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kProcessPerTab);
  StartEmbeddedServer();

  const GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetRenderProcessHost()->GetID()));

  // Capture the SiteInstance before navigating to about:blank to ensure
  // it doesn't change.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());

  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  EXPECT_NE(orig_site_instance, shell()->web_contents()->GetSiteInstance());

  GURL regular_page_url(embedded_test_server()->GetURL("/title2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), regular_page_url));
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
      shell()->web_contents()->GetRenderProcessHost()->GetID()));
}

// crbug.com/615274
// This test ensures that after an RFH is swapped out, the associated WebUI
// instance is no longer allowed to send JavaScript messages. This is necessary
// because WebUI currently (and unusually) always sends JavaScript messages to
// the current main frame, rather than the RFH that owns the WebUI.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       WebUIJavascriptDisallowedAfterSwapOut) {
  StartEmbeddedServer();

  const GURL web_ui_url(std::string(kChromeUIScheme) + "://" +
                        std::string(kChromeUIGpuHost));
  EXPECT_TRUE(NavigateToURL(shell(), web_ui_url));

  RenderFrameHostImpl* rfh =
      static_cast<WebContentsImpl*>(shell()->web_contents())->GetMainFrame();

  // Set up a slow unload handler to force the RFH to linger in the swapped
  // out but not-yet-deleted state.
  EXPECT_TRUE(
      ExecuteScript(rfh, "window.onunload=function(e){ while(1); };\n"));

  WebUIImpl* web_ui = rfh->web_ui();

  EXPECT_TRUE(web_ui->CanCallJavascript());
  auto handler_owner = base::MakeUnique<TestWebUIMessageHandler>();
  TestWebUIMessageHandler* handler = handler_owner.get();

  web_ui->AddMessageHandler(std::move(handler_owner));
  EXPECT_FALSE(handler->IsJavascriptAllowed());

  handler->AllowJavascript();
  EXPECT_TRUE(handler->IsJavascriptAllowed());

  rfh->DisableSwapOutTimerForTesting();
  RenderFrameHostDestructionObserver rfh_observer(rfh);

  // Navigate, but wait for commit, not the actual load to finish.
  SiteInstanceImpl* web_ui_site_instance = rfh->GetSiteInstance();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(GURL(url::kAboutBlankURL));
  commit_observer.WaitForCommit();
  EXPECT_NE(web_ui_site_instance, shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(
      root->render_manager()->GetRenderFrameProxyHost(web_ui_site_instance));

  // The previous RFH should still be pending deletion, as we wait for either
  // the SwapOut ACK or a timeout.
  ASSERT_TRUE(rfh->IsRenderFrameLive());
  ASSERT_FALSE(rfh->is_active());

  // We specifically want verify behavior between swap-out and RFH destruction.
  ASSERT_FALSE(rfh_observer.deleted());

  EXPECT_FALSE(handler->IsJavascriptAllowed());
}

// Test for http://crbug.com/703303.  Ensures that the renderer process does not
// try to select files whose paths cannot be converted to WebStrings.  This
// check is done in the renderer because it is hard to predict which paths will
// turn into empty WebStrings, and the behavior varies by platform.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, DontSelectInvalidFiles) {
  StartServer();

  // Use a file path with an invalid encoding, such that it can't be converted
  // to a WebString (on all platforms but Windows).
  base::FilePath file;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file));
  file = file.Append(FILE_PATH_LITERAL("foo\337bar"));

  // Navigate and try to get page to reference this file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input.html"));
  NavigateToURL(shell(), url1);
  int process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(file));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());

  // The browser process grants access to the file whether or not the renderer
  // process realizes that it can't use it.  This is ok, since the user actually
  // did select the file from the chooser.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Disable the swap out timer so we wait for the UpdateState message.
  static_cast<WebContentsImpl*>(shell()->web_contents())
      ->GetMainFrame()
      ->DisableSwapOutTimerForTesting();

  // Navigate to a different process and wait for the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetRenderProcessHost(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), GetCrossSiteURL("/title1.html"));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // The renderer process should not have been killed.  This is the important
  // part of the test.  If this fails, then we didn't get a PageState to check
  // below, so use an assert (since the test can't meaningfully proceed).
  ASSERT_TRUE(exit_observer.did_exit_normally());

  // Ensure that the file did not end up in the PageState of the previous entry,
  // except on Windows where the path is valid and WebString can handle it.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
#if defined(OS_WIN)
  EXPECT_EQ(1U, files.size());
#else
  EXPECT_EQ(0U, files.size());
#endif
}

// Test for http://crbug.com/262948.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RestoreFileAccessForHistoryNavigation) {
  StartServer();
  base::FilePath file;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input.html"));
  NavigateToURL(shell(), url1);
  int process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(file));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Disable the swap out timer so we wait for the UpdateState message.
  static_cast<WebContentsImpl*>(shell()->web_contents())
      ->GetMainFrame()
      ->DisableSwapOutTimerForTesting();

  // Navigate to a different process without access to the file, and wait for
  // the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetRenderProcessHost(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), GetCrossSiteURL("/title1.html"));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Ensure that the file ended up in the PageState of the previous entry.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, files.size());
  EXPECT_EQ(file, files.at(0));

  // Go back, ending up in a different RenderProcessHost than before.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();
  EXPECT_NE(process_id,
            shell()->web_contents()->GetRenderProcessHost()->GetID());

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Navigate to a same site page to trigger a PageState update and ensure the
  // renderer is not killed.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));
}

// Test for http://crbug.com/441966.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RestoreSubframeFileAccessForHistoryNavigation) {
  StartServer();
  base::FilePath file;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");

  // Navigate to url and get it to reference a file in its PageState.
  GURL url1(embedded_test_server()->GetURL("/file_input_subframe.html"));
  NavigateToURL(shell(), url1);
  WebContentsImpl* wc = static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = wc->GetFrameTree()->root();
  int process_id = shell()->web_contents()->GetRenderProcessHost()->GetID();
  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(file));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      process_id, file));

  // Disable the swap out timer so we wait for the UpdateState message.
  root->current_frame_host()->DisableSwapOutTimerForTesting();

  // Do an in-page navigation in the child to make sure we hear a PageState with
  // the chosen file before the subframe's FrameTreeNode is deleted.  In
  // practice, we'll get the PageState 1 second after the file is chosen.
  // TODO(creis): Remove this in-page navigation once we keep track of
  // FrameTreeNodes that are pending deletion.  See https://crbug.com/609963.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    std::string script = "location.href='#foo';";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    nav_observer.Wait();
  }

  // Navigate to a different process without access to the file, and wait for
  // the old process to exit.
  RenderProcessHostWatcher exit_observer(
      shell()->web_contents()->GetRenderProcessHost(),
      RenderProcessHostWatcher::WATCH_FOR_HOST_DESTRUCTION);
  NavigateToURL(shell(), GetCrossSiteURL("/title1.html"));
  exit_observer.Wait();
  EXPECT_FALSE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Ensure that the file ended up in the PageState of the previous entry.
  NavigationEntry* prev_entry =
      shell()->web_contents()->GetController().GetEntryAtIndex(0);
  EXPECT_EQ(url1, prev_entry->GetURL());
  const std::vector<base::FilePath>& files =
      prev_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, files.size());
  EXPECT_EQ(file, files.at(0));

  // Go back, ending up in a different RenderProcessHost than before.
  TestNavigationObserver back_nav_load_observer(shell()->web_contents());
  shell()->web_contents()->GetController().GoToIndex(0);
  back_nav_load_observer.Wait();
  EXPECT_NE(process_id,
            shell()->web_contents()->GetRenderProcessHost()->GetID());

  // Ensure that the file access still exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      shell()->web_contents()->GetRenderProcessHost()->GetID(), file));

  // Do another in-page navigation in the child to make sure we hear a PageState
  // with the chosen file.
  // TODO(creis): Remove this in-page navigation once we keep track of
  // FrameTreeNodes that are pending deletion.  See https://crbug.com/609963.
  {
    TestNavigationObserver nav_observer(shell()->web_contents());
    std::string script = "location.href='#foo';";
    EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
    nav_observer.Wait();
  }

  // Also try cloning the tab by creating a new NavigationEntry with the same
  // PageState.  This exercises a different path, by combining the frame
  // specific PageStates into a full-tree PageState and converting back.  There
  // was a bug where this caused us to lose the list of referenced files.  See
  // https://crbug.com/620261.
  std::unique_ptr<NavigationEntryImpl> cloned_entry =
      NavigationEntryImpl::FromNavigationEntry(
          NavigationControllerImpl::CreateNavigationEntry(
              url1, Referrer(), ui::PAGE_TRANSITION_RELOAD, false,
              std::string(), shell()->web_contents()->GetBrowserContext()));
  prev_entry = shell()->web_contents()->GetController().GetEntryAtIndex(0);
  cloned_entry->SetPageState(prev_entry->GetPageState());
  const std::vector<base::FilePath>& cloned_files =
      cloned_entry->GetPageState().GetReferencedFiles();
  ASSERT_EQ(1U, cloned_files.size());
  EXPECT_EQ(file, cloned_files.at(0));

  std::vector<std::unique_ptr<NavigationEntry>> entries;
  entries.push_back(std::move(cloned_entry));
  Shell* new_shell =
      Shell::CreateNewWindow(shell()->web_contents()->GetBrowserContext(),
                             GURL::EmptyGURL(), nullptr, gfx::Size());
  FrameTreeNode* new_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  NavigationControllerImpl& new_controller =
      static_cast<NavigationControllerImpl&>(
          new_shell->web_contents()->GetController());
  new_controller.Restore(entries.size() - 1,
                         RestoreType::LAST_SESSION_EXITED_CLEANLY, &entries);
  ASSERT_EQ(0u, entries.size());
  {
    TestNavigationObserver restore_observer(new_shell->web_contents());
    new_controller.LoadIfNecessary();
    restore_observer.Wait();
  }
  ASSERT_EQ(1U, new_root->child_count());
  EXPECT_EQ(url1, new_root->current_url());

  // Ensure that the file access exists in the new process ID.
  EXPECT_TRUE(ChildProcessSecurityPolicyImpl::GetInstance()->CanReadFile(
      new_root->current_frame_host()->GetProcess()->GetID(), file));

  // Also, extract the file from the renderer process to ensure that the
  // response made it over successfully and the proper filename is set.
  std::string file_name;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      new_root->child_at(0),
      "window.domAutomationController.send("
      "document.getElementById('fileinput').files[0].name);",
      &file_name));
  EXPECT_EQ("bar", file_name);

  // Navigate to a same site page to trigger a PageState update and ensure the
  // renderer is not killed.
  EXPECT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title2.html")));
}

// Ensures that no RenderFrameHost/RenderViewHost objects are leaked when
// doing a simple cross-process navigation.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CleanupOnCrossProcessNavigation) {
  StartEmbeddedServer();

  // Do an initial navigation and capture objects we expect to be cleaned up
  // on cross-process navigation.
  GURL start_url = embedded_test_server()->GetURL("/title1.html");
  NavigateToURL(shell(), start_url);

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  int32_t orig_site_instance_id =
      root->current_frame_host()->GetSiteInstance()->GetId();
  int initial_process_id =
      root->current_frame_host()->GetSiteInstance()->GetProcess()->GetID();
  int initial_rfh_id = root->current_frame_host()->GetRoutingID();
  int initial_rvh_id =
      root->current_frame_host()->render_view_host()->GetRoutingID();

  // Navigate cross-process and ensure that cleanup is performed as expected.
  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/title2.html");
  RenderFrameHostDestructionObserver rfh_observer(root->current_frame_host());
  NavigateToURL(shell(), cross_site_url);
  rfh_observer.Wait();

  EXPECT_NE(orig_site_instance_id,
            root->current_frame_host()->GetSiteInstance()->GetId());
  EXPECT_FALSE(RenderFrameHost::FromID(initial_process_id, initial_rfh_id));
  EXPECT_FALSE(RenderViewHost::FromID(initial_process_id, initial_rvh_id));
}

// Ensure that the opener chain proxies and RVHs are properly reinitialized if
// a tab crashes and reloads.  See https://crbug.com/505090.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ReinitializeOpenerChainAfterCrashAndReload) {
  StartEmbeddedServer();

  GURL main_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance);

  // Open a popup and navigate it cross-site.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(new_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();

  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/title2.html");
  EXPECT_TRUE(NavigateToURL(new_shell, cross_site_url));

  scoped_refptr<SiteInstance> foo_site_instance(
      new_shell->web_contents()->GetSiteInstance());
  EXPECT_NE(foo_site_instance, orig_site_instance);

  // Kill the popup's process.
  RenderProcessHost* popup_process =
      popup_root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      popup_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  popup_process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(
      popup_root->current_frame_host()->render_view_host()->IsRenderViewLive());

  // The swapped-out RVH and proxy for the opener page in the foo.com
  // SiteInstance should not be live.
  RenderFrameHostManager* opener_manager = root->render_manager();
  RenderViewHostImpl* opener_rvh =
      opener_manager->GetSwappedOutRenderViewHost(foo_site_instance.get());
  EXPECT_TRUE(opener_rvh);
  EXPECT_FALSE(opener_rvh->IsRenderViewLive());
  RenderFrameProxyHost* opener_rfph =
      opener_manager->GetRenderFrameProxyHost(foo_site_instance.get());
  EXPECT_TRUE(opener_rfph);
  EXPECT_FALSE(opener_rfph->is_render_frame_proxy_live());

  // Re-navigate the popup to the same URL and check that this recreates the
  // opener's swapped out RVH and proxy in the foo.com SiteInstance.
  EXPECT_TRUE(NavigateToURL(new_shell, cross_site_url));
  EXPECT_TRUE(opener_rvh->IsRenderViewLive());
  EXPECT_TRUE(opener_rfph->is_render_frame_proxy_live());
}

// Test that when a frame's opener is updated via window.open, the browser
// process and the frame's proxies in other processes find out about the new
// opener.  Open two popups in different processes, set one popup's opener to
// the other popup, and ensure that the opener is updated in all processes.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, UpdateOpener) {
  StartEmbeddedServer();

  GURL main_url = embedded_test_server()->GetURL("/post_message.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_TRUE(orig_site_instance);

  // Open a cross-site popup named "foo" and a same-site popup named "bar".
  Shell* foo_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/post_message.html"));
  NavigateToURL(foo_shell, foo_url);

  GURL bar_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_post_message_frames.html"));
  Shell* bar_shell = OpenPopup(shell(), bar_url, "bar");
  EXPECT_TRUE(bar_shell);

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            foo_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            bar_shell->web_contents()->GetSiteInstance());

  // Initially, both popups' openers should point to main window.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();
  FrameTreeNode* bar_root =
      static_cast<WebContentsImpl*>(bar_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(root, foo_root->opener());
  EXPECT_EQ(root, foo_root->original_opener());
  EXPECT_EQ(root, bar_root->opener());
  EXPECT_EQ(root, bar_root->original_opener());

  // From the bar process, use window.open to update foo's opener to point to
  // bar. This is allowed since bar is same-origin with foo's opener.  Use
  // window.open with an empty URL, which should return a reference to the
  // target frame without navigating it.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      bar_shell,
      "window.domAutomationController.send(!!window.open('','foo'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_FALSE(foo_shell->web_contents()->IsLoading());
  EXPECT_EQ(foo_url, foo_root->current_url());

  // Check that updated opener propagated to the browser process.
  EXPECT_EQ(bar_root, foo_root->opener());
  EXPECT_EQ(root, foo_root->original_opener());

  // Check that foo's opener was updated in foo's process. Send a postMessage
  // to the opener and check that the right window (bar_shell) receives it.
  base::string16 expected_title = ASCIIToUTF16("opener-msg");
  TitleWatcher title_watcher(bar_shell->web_contents(), expected_title);
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      foo_shell,
      "window.domAutomationController.send(postToOpener('opener-msg', '*'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that a non-null assignment to the opener doesn't change the opener
  // in the browser process.
  EXPECT_TRUE(ExecuteScript(foo_shell, "window.opener = window;"));
  EXPECT_EQ(bar_root, foo_root->opener());
  EXPECT_EQ(root, foo_root->original_opener());
}

// Tests that when a popup is opened, which is then navigated cross-process and
// back, it can be still accessed through the original window reference in
// JavaScript. See https://crbug.com/537657
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PopupKeepsWindowReferenceCrossProcesAndBack) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToPageWithLinks(shell());

  // Click a target=foo link to open a popup.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();
  EXPECT_TRUE(new_shell->web_contents()->HasOpener());

  // Wait for the navigation in the popup to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Capture the window reference, so we can check that accessing its location
  // works after navigating cross-process and back.
  GURL expected_url = new_shell->web_contents()->GetLastCommittedURL();
  EXPECT_TRUE(ExecuteScript(shell(), "saveWindowReference();"));

  // Now navigate the popup to a different site and then go back.
  NavigateToURL(new_shell,
                embedded_test_server()->GetURL("foo.com", "/title1.html"));
  TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
  new_shell->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();

  // Check that the location.href window attribute is accessible and is correct.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send(getLastOpenedWindowLocation());",
      &result));
  EXPECT_EQ(expected_url.spec(), result);
}

// Tests that going back to the same SiteInstance as a pending RenderFrameHost
// doesn't create a duplicate RenderFrameProxyHost. For example:
// 1. Navigate to a page on the opener site - a.com
// 2. Navigate to a page on site b.com
// 3. Start a navigation to another page on a.com, but commit is delayed.
// 4. Go back.
// See https://crbug.com/541619.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       PopupPendingAndBackToSameSiteInstance) {
  StartEmbeddedServer();
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateToURL(shell(), main_url);

  // Open a popup to navigate.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the popup to a different site.
  NavigateToURL(new_shell,
                embedded_test_server()->GetURL("b.com", "/title2.html"));

  // Navigate again to the original site, but to a page that will take a while
  // to commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  NavigationStallDelegate stall_delegate(same_site_url);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  new_shell->LoadURL(same_site_url);

  // Going back in history should work and the test should not crash.
  TestNavigationObserver back_nav_load_observer(new_shell->web_contents());
  new_shell->web_contents()->GetController().GoBack();
  back_nav_load_observer.Wait();

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

// Tests that InputMsg type IPCs are ignored by swapped out RenderViews. It
// uses the SetFocus IPC, as RenderView has a CHECK to ensure that condition
// never happens.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       InputMsgToSwappedOutRVHIsIgnored) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup to navigate cross-process.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Keep a pointer to the RenderViewHost, which will be in swapped out
  // state after navigating cross-process. This is how this test is causing
  // a swapped out RenderView to receive InputMsg IPC message.
  WebContentsImpl* new_web_contents =
      static_cast<WebContentsImpl*>(new_shell->web_contents());
  FrameTreeNode* new_root = new_web_contents->GetFrameTree()->root();
  RenderViewHostImpl* rvh = new_web_contents->GetRenderViewHost();

  // Navigate the popup to a different site, so the |rvh| is swapped out.
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("b.com", "/title2.html")));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());
  EXPECT_EQ(rvh, new_root->render_manager()->GetSwappedOutRenderViewHost(
                     shell()->web_contents()->GetSiteInstance()));

  // Setup a process observer to ensure there is no crash and send the IPC
  // message.
  RenderProcessHostWatcher watcher(
      rvh->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rvh->Send(new InputMsg_SetFocus(rvh->GetRoutingID(), true));

  // The test must wait for a process to exit, but if the IPC message is
  // properly ignored, there will be no crash. Therefore, navigate the
  // original window to the same site as the popup, which will just exit the
  // process cleanly.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

// Tests that navigating cross-process and reusing an existing RenderViewHost
// (whose process has been killed/crashed) recreates properly the RenderView and
// RenderFrameProxy on the renderer side.
// See https://crbug.com/544271
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RenderViewInitAfterProcessKill) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup to navigate.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the popup to a different site.
  EXPECT_TRUE(NavigateToURL(
      new_shell, embedded_test_server()->GetURL("b.com", "/title2.html")));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Kill the process hosting the popup.
  RenderProcessHost* process = popup_root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(
      popup_root->current_frame_host()->render_view_host()->IsRenderViewLive());

  // Navigate the main tab to the site of the popup. This will cause the
  // RenderView for b.com in the main tab to be recreated. If the issue
  // is not fixed, this will result in process crash and failing test.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));
}

// Ensure that we don't crash the renderer in CreateRenderView if a proxy goes
// away between swapout and the next navigation.  See https://crbug.com/581912.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CreateRenderViewAfterProcessKillAndClosedProxy) {
  StartEmbeddedServer();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Give an initial page an unload handler that never completes.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(
      ExecuteScript(root, "window.onunload=function(e){ while(1); };\n"));

  // Open a popup in the same process.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the first tab to a different site, and only wait for commit, not
  // load stop.
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();
  SiteInstanceImpl* site_instance_a = rfh_a->GetSiteInstance();
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title2.html"));
  commit_observer.WaitForCommit();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // The previous RFH should still be pending deletion, as we wait for either
  // the SwapOut ACK or a timeout.
  ASSERT_TRUE(rfh_a->IsRenderFrameLive());
  ASSERT_FALSE(rfh_a->is_active());

  // The corresponding RVH should still be referenced by the proxy and the old
  // frame.
  RenderViewHostImpl* rvh_a = rfh_a->render_view_host();
  EXPECT_EQ(2, rvh_a->ref_count());

  // Kill the old process.
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(popup_root->current_frame_host()->IsRenderFrameLive());
  // |rfh_a| is now deleted, thanks to the bug fix.

  // With |rfh_a| gone, the RVH should only be referenced by the (dead) proxy.
  EXPECT_EQ(1, rvh_a->ref_count());
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));
  EXPECT_FALSE(root->render_manager()
                   ->GetRenderFrameProxyHost(site_instance_a)
                   ->is_render_frame_proxy_live());

  // Close the popup so there is no proxy for a.com in the original tab.
  new_shell->Close();
  EXPECT_FALSE(
      root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // This should delete the RVH as well.
  EXPECT_FALSE(root->frame_tree()->GetRenderViewHost(site_instance_a));

  // Go back in the main frame from b.com to a.com. In https://crbug.com/581912,
  // the browser process would crash here because there was no main frame
  // routing ID or proxy in RVHI::CreateRenderView.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }
}

// Ensure that we don't crash in RenderViewImpl::Init if a proxy is created
// after swapout and before navigation.  See https://crbug.com/544755.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       RenderViewInitAfterNewProxyAndProcessKill) {
  StartEmbeddedServer();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Give an initial page an unload handler that never completes.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  EXPECT_TRUE(
      ExecuteScript(root, "window.onunload=function(e){ while(1); };\n"));

  // Navigate the tab to a different site, and only wait for commit, not load
  // stop.
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();
  SiteInstanceImpl* site_instance_a = rfh_a->GetSiteInstance();
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title2.html"));
  commit_observer.WaitForCommit();
  EXPECT_NE(site_instance_a, shell()->web_contents()->GetSiteInstance());

  // The previous RFH should still be pending deletion, as we wait for either
  // the SwapOut ACK or a timeout.
  ASSERT_TRUE(rfh_a->IsRenderFrameLive());
  ASSERT_FALSE(rfh_a->is_active());

  // When the previous RFH was swapped out, it should have still gotten a
  // replacement proxy even though it's the last active frame in the process.
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Open a popup in the new B process.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            new_shell->web_contents()->GetSiteInstance());

  // Navigate the popup to the original site, but don't wait for commit (which
  // won't happen).  This should reuse the proxy in the original tab, which at
  // this point exists alongside the RFH pending deletion.
  new_shell->LoadURL(embedded_test_server()->GetURL("a.com", "/title2.html"));
  EXPECT_TRUE(root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Kill the old process.
  RenderProcessHost* process = rfh_a->GetProcess();
  RenderProcessHostWatcher crash_observer(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process->Shutdown(0, false);
  crash_observer.Wait();
  // |rfh_a| is now deleted, thanks to the bug fix.

  // Go back in the main frame from b.com to a.com.
  {
    TestNavigationObserver back_nav_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_nav_load_observer.Wait();
  }

  // In https://crbug.com/581912, the renderer process would crash here because
  // there was a frame, view, and proxy (and is_swapped_out was true).
  EXPECT_EQ(site_instance_a, root->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());
  EXPECT_TRUE(new_shell->web_contents()->GetMainFrame()->IsRenderFrameLive());
}

// Ensure that we use the same pending RenderFrameHost if a second navigation to
// its site occurs before it commits.  Otherwise the renderer process will have
// two competing pending RenderFrames that both try to swap with the same
// RenderFrameProxy.  See https://crbug.com/545900.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ConsecutiveNavigationsToSite) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to b.com to keep the b.com process alive.
  Shell* new_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "popup");
  NavigateToURL(new_shell,
                embedded_test_server()->GetURL("b.com", "/title3.html"));

  // Start a cross-site navigation to the same site but don't commit.
  GURL cross_site_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigationStallDelegate stall_delegate(cross_site_url);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  shell()->LoadURL(cross_site_url);

  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      shell()->web_contents());
  RenderFrameHostImpl* next_rfh =
      IsBrowserSideNavigationEnabled()
          ? web_contents->GetRenderManagerForTesting()->speculative_frame_host()
          : web_contents->GetRenderManagerForTesting()->pending_frame_host();
  ASSERT_TRUE(next_rfh);

  // Navigate to the same new site and verify that we commit in the same RFH.
  GURL cross_site_url2(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationObserver navigation_observer(web_contents, 1);
  shell()->LoadURL(cross_site_url2);
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_EQ(
        next_rfh,
        web_contents->GetRenderManagerForTesting()->speculative_frame_host());
  } else {
    EXPECT_EQ(next_rfh,
              web_contents->GetRenderManagerForTesting()->pending_frame_host());
  }
  navigation_observer.Wait();
  EXPECT_EQ(cross_site_url2, web_contents->GetLastCommittedURL());
  EXPECT_EQ(next_rfh, web_contents->GetMainFrame());
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_FALSE(
        web_contents->GetRenderManagerForTesting()->speculative_frame_host());
  } else {
    EXPECT_FALSE(
        web_contents->GetRenderManagerForTesting()->pending_frame_host());
  }

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

// Check that if a sandboxed subframe opens a cross-process popup such that the
// popup's opener won't be set, the popup still inherits the subframe's sandbox
// flags.  This matters for rel=noopener and rel=noreferrer links, as well as
// for some situations in non-site-per-process mode where the popup would
// normally maintain the opener, but loses it due to being placed in a new
// process and not creating subframe proxies.  The latter might happen when
// opening the default search provider site.  See https://crbug.com/576204.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       CrossProcessPopupInheritsSandboxFlagsWithNoOpener) {
  StartEmbeddedServer();
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Add a sandboxed about:blank iframe.
  {
    std::string script =
        "var frame = document.createElement('iframe');\n"
        "frame.sandbox = 'allow-scripts allow-popups';\n"
        "document.body.appendChild(frame);\n";
    EXPECT_TRUE(ExecuteScript(shell(), script));
  }

  // Navigate iframe to a page with target=_blank links, and rewrite the links
  // to point to valid cross-site URLs.
  GURL frame_url(
      embedded_test_server()->GetURL("a.com", "/click-noreferrer-links.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  std::string script = "setOriginForLinks('http://b.com:" +
                       embedded_test_server()->base_url().port() + "/');";
  EXPECT_TRUE(ExecuteScript(root->child_at(0), script));

  // Helper to click on the 'rel=noreferrer target=_blank' and 'rel=noopener
  // target=_blank' links.  Checks that these links open a popup that ends up
  // in a new SiteInstance even without site-per-process and then verifies that
  // the popup is still sandboxed.
  auto click_link_and_verify_popup = [this,
                                      root](std::string link_opening_script) {
    ShellAddedObserver new_shell_observer;
    bool success = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        root->child_at(0),
        "window.domAutomationController.send(" + link_opening_script + ")",
        &success));
    EXPECT_TRUE(success);

    Shell* new_shell = new_shell_observer.GetShell();
    EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));
    EXPECT_NE(new_shell->web_contents()->GetSiteInstance(),
              shell()->web_contents()->GetSiteInstance());

    // Check that the popup is sandboxed by checking its document.origin, which
    // should be unique.
    std::string origin;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        new_shell, "domAutomationController.send(document.origin)", &origin));
    EXPECT_EQ("null", origin);
  };

  click_link_and_verify_popup("clickNoOpenerTargetBlankLink()");
  click_link_and_verify_popup("clickNoRefTargetBlankLink()");
}

// When two frames are same-origin but cross-process, they should behave as if
// they are not same-origin and should not crash.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SameOriginFramesInDifferentProcesses) {
  StartEmbeddedServer();

  // Load a page with links that open in a new window.
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "a.com", "/click-noreferrer-links.html"));

  // Get the original SiteInstance for later comparison.
  scoped_refptr<SiteInstance> orig_site_instance(
      shell()->web_contents()->GetSiteInstance());
  EXPECT_NE(nullptr, orig_site_instance.get());

  // Test clicking a target=foo link.
  ShellAddedObserver new_shell_observer;
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      shell(),
      "window.domAutomationController.send(clickSameSiteTargetedLink());"
      "saveWindowReference();",
      &success));
  EXPECT_TRUE(success);
  Shell* new_shell = new_shell_observer.GetShell();

  // Wait for the navigation in the new tab to finish, if it hasn't.
  WaitForLoadStop(new_shell->web_contents());
  EXPECT_EQ("/navigate_opener.html",
            new_shell->web_contents()->GetLastCommittedURL().path());

  // Do a cross-site navigation that winds up same-site. The same-site
  // navigation to a.com will commit in a different process than the original
  // a.com window.
  NavigateToURL(new_shell, embedded_test_server()->GetURL(
                               "b.com", "/cross-site/a.com/title1.html"));
  if (AreAllSitesIsolatedForTesting() || IsBrowserSideNavigationEnabled()) {
    // In --site-per-process mode, both windows will actually be in the same
    // process.
    // PlzNavigate: the SiteInstance for the navigation is determined after the
    // redirect. So both windows will actually be in the same process.
    EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
              new_shell->web_contents()->GetSiteInstance());
  } else {
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
              new_shell->web_contents()->GetSiteInstance());
  }

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      shell(),
      "window.domAutomationController.send((function() {\n"
      "  try {\n"
      "    return getLastOpenedWindowLocation();\n"
      "  } catch (e) {\n"
      "    return e.toString();\n"
      "  }\n"
      "})())",
      &result));
  if (AreAllSitesIsolatedForTesting() || IsBrowserSideNavigationEnabled()) {
    EXPECT_THAT(result,
                ::testing::MatchesRegex("http://a.com:\\d+/title1.html"));
  } else {
    // Accessing a property with normal security checks should throw a
    // SecurityError if the same-origin windows are in different processes.
    EXPECT_THAT(result,
                ::testing::MatchesRegex("SecurityError: Blocked a frame with "
                                        "origin \"http://a.com:\\d+\" from "
                                        "accessing a cross-origin frame."));
  }
}

// Test coverage for attempts to open subframe links in new windows, to prevent
// incorrect invariant checks.  See https://crbug.com/605055.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, CtrlClickSubframeLink) {
  StartEmbeddedServer();

  // Load a page with a subframe link.
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             "/ctrl-click-subframe-link.html"));

  // Simulate a ctrl click on the link.  This won't actually create a new Shell
  // because Shell::OpenURLFromTab only supports CURRENT_TAB, but it's enough to
  // trigger the crash from https://crbug.com/605055.
  EXPECT_TRUE(ExecuteScript(
      shell(), "window.domAutomationController.send(ctrlClickLink());"));
}

// Ensure that we don't update the wrong NavigationEntry's title after an
// ignored commit during a cross-process navigation.
// See https://crbug.com/577449.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       UnloadPushStateOnCrossProcessNavigation) {
  StartEmbeddedServer();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // Give an initial page an unload handler that does a pushState, which will be
  // ignored by the browser process.  It then does a title update which is
  // meant for a NavigationEntry that will never be created.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html")));
  EXPECT_TRUE(ExecuteScript(root,
                            "window.onunload=function(e){"
                            "history.pushState({}, 'foo', 'foo');"
                            "document.title='foo'; };\n"));
  base::string16 title = web_contents->GetTitle();
  NavigationEntryImpl* entry = web_contents->GetController().GetEntryAtIndex(0);

  // Navigate the first tab to a different site and wait for the old process to
  // complete its unload handler and exit.
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();
  RenderProcessHostWatcher exit_observer(
      rfh_a->GetProcess(), RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  TestNavigationObserver commit_observer(web_contents);
  shell()->LoadURL(embedded_test_server()->GetURL("b.com", "/title1.html"));
  commit_observer.Wait();
  exit_observer.Wait();

  // Ensure the entry's title hasn't changed after the ignored commit.
  EXPECT_EQ(title, entry->GetTitle());
}

// Ensure that document hosted on file: URL can successfully execute pushState
// with arbitrary origin, when universal access setting is enabled.
// TODO(nasko): The test is disabled on Mac, since universal access from file
// scheme behaves differently.
#if defined(OS_MACOSX)
#define MAYBE_EnsureUniversalAccessFromFileSchemeSucceeds \
  DISABLED_EnsureUniversalAccessFromFileSchemeSucceeds
#else
#define MAYBE_EnsureUniversalAccessFromFileSchemeSucceeds \
  EnsureUniversalAccessFromFileSchemeSucceeds
#endif
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       MAYBE_EnsureUniversalAccessFromFileSchemeSucceeds) {
  StartEmbeddedServer();
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  WebPreferences prefs =
      web_contents->GetRenderViewHost()->GetWebkitPreferences();
  prefs.allow_universal_access_from_file_urls = true;
  web_contents->GetRenderViewHost()->UpdateWebkitPreferences(prefs);

  GURL file_url = GetTestUrl("", "title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), file_url));
  EXPECT_EQ(1, web_contents->GetController().GetEntryCount());
  EXPECT_TRUE(ExecuteScript(
      root, "window.history.pushState({}, '', 'https://chromium.org');"));
  EXPECT_EQ(2, web_contents->GetController().GetEntryCount());
  EXPECT_TRUE(web_contents->GetMainFrame()->IsRenderFrameLive());
}

// Ensure that navigating back from a sad tab to an existing process works
// correctly. See https://crbug.com/591984.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       NavigateBackToExistingProcessFromSadTab) {
  StartEmbeddedServer();
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(
      shell(), embedded_test_server()->GetURL("a.com", "/title2.html"), "foo");
  EXPECT_TRUE(NavigateToURL(
      popup, embedded_test_server()->GetURL("b.com", "/title3.html")));

  // Kill the b.com process.
  RenderProcessHost* b_process =
      popup->web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      b_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  b_process->Shutdown(0, false);
  crash_observer.Wait();

  // The popup should now be showing the sad tab.  Main tab should not be.
  EXPECT_NE(base::TERMINATION_STATUS_STILL_RUNNING,
            popup->web_contents()->GetCrashedStatus());
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            shell()->web_contents()->GetCrashedStatus());

  // Go back in the popup from b.com to a.com/title2.html.
  TestNavigationObserver back_observer(popup->web_contents());
  popup->web_contents()->GetController().GoBack();
  back_observer.Wait();

  // In the bug, after the back navigation the popup was still showing
  // the sad tab.  Ensure this is not the case.
  EXPECT_EQ(base::TERMINATION_STATUS_STILL_RUNNING,
            popup->web_contents()->GetCrashedStatus());
  EXPECT_TRUE(popup->web_contents()->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(popup->web_contents()->GetMainFrame()->GetSiteInstance(),
            shell()->web_contents()->GetMainFrame()->GetSiteInstance());
}

// Verify that GetLastCommittedOrigin() is correct for the full lifetime of a
// RenderFrameHost, including when it's pending, current, and pending deletion.
// This is checked both for main frames and subframes.
// See https://crbug.com/590035.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, LastCommittedOrigin) {
  StartEmbeddedServer();
  GURL url_a(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_a));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  RenderFrameHostImpl* rfh_a = root->current_frame_host();
  rfh_a->DisableSwapOutTimerForTesting();

  EXPECT_EQ(url::Origin(url_a), rfh_a->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_a, web_contents->GetMainFrame());

  // Start a navigation to a b.com URL, and don't wait for commit.
  GURL url_b(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(root);
  RenderFrameDeletedObserver deleted_observer(rfh_a);
  shell()->LoadURL(url_b);

  // The pending RFH shouln't have a last committed origin (the default value
  // is a unique origin). The current RFH shouldn't change its last committed
  // origin before commit.
  RenderFrameHostImpl* rfh_b =
      IsBrowserSideNavigationEnabled()
          ? root->render_manager()->speculative_frame_host()
          : root->render_manager()->pending_frame_host();
  EXPECT_EQ("null", rfh_b->GetLastCommittedOrigin().Serialize());
  EXPECT_EQ(url::Origin(url_a), rfh_a->GetLastCommittedOrigin());

  // Verify that the last committed origin is set for the b.com RHF once it
  // commits.
  commit_observer.WaitForCommit();
  EXPECT_EQ(url::Origin(url_b), rfh_b->GetLastCommittedOrigin());
  EXPECT_EQ(rfh_b, web_contents->GetMainFrame());

  // The old RFH should now be pending deletion.  Verify it still has correct
  // last committed origin.
  EXPECT_EQ(url::Origin(url_a), rfh_a->GetLastCommittedOrigin());
  EXPECT_FALSE(rfh_a->is_active());

  // Wait for |rfh_a| to be deleted and double-check |rfh_b|'s origin.
  deleted_observer.WaitUntilDeleted();
  EXPECT_EQ(url::Origin(url_b), rfh_b->GetLastCommittedOrigin());

  // Navigate to a same-origin page with an about:blank iframe.  The iframe
  // should also have a b.com origin.
  GURL url_b_with_frame(embedded_test_server()->GetURL(
      "b.com", "/navigation_controller/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_b_with_frame));
  EXPECT_EQ(rfh_b, web_contents->GetMainFrame());
  EXPECT_EQ(url::Origin(url_b), rfh_b->GetLastCommittedOrigin());
  FrameTreeNode* child = root->child_at(0);
  RenderFrameHostImpl* child_rfh_b = root->child_at(0)->current_frame_host();
  child_rfh_b->DisableSwapOutTimerForTesting();
  EXPECT_EQ(url::Origin(url_b), child_rfh_b->GetLastCommittedOrigin());

  // Navigate subframe to c.com.  Wait for commit but not full load, and then
  // verify the subframe's origin.
  GURL url_c(embedded_test_server()->GetURL("c.com", "/title3.html"));
  {
    TestFrameNavigationObserver commit_observer(root->child_at(0));
    EXPECT_TRUE(
        ExecuteScript(child, "location.href = '" + url_c.spec() + "';"));
    commit_observer.WaitForCommit();
  }
  EXPECT_EQ(url::Origin(url_c),
            child->current_frame_host()->GetLastCommittedOrigin());

  // With OOPIFs, this navigation used a cross-process transfer.  Ensure that
  // the iframe's old RFH still has correct origin, even though it's pending
  // deletion.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_FALSE(child_rfh_b->is_active());
    EXPECT_NE(child_rfh_b, child->current_frame_host());
    EXPECT_EQ(url::Origin(url_b), child_rfh_b->GetLastCommittedOrigin());
  }
}

// Verify that with Site Isolation enabled, chrome:// pages with subframes
// to other chrome:// URLs all stay in the same process.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       ChromeSchemeSubframesStayInProcessWithParent) {
  // Enable Site Isolation so subframes with different chrome:// URLs will be
  // treated as cross-site.
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  StartEmbeddedServer();

  GURL chrome_top_url = GURL(std::string(kChromeUIScheme) + "://" +
                             std::string(kChromeUIBlobInternalsHost));
  GURL chrome_child_url = GURL(std::string(kChromeUIScheme) + "://" +
                               std::string(kChromeUIHistogramHost));
  GURL regular_web_url(embedded_test_server()->GetURL("/title1.html"));

  NavigationControllerImpl& controller = static_cast<NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Navigate the main frame to the top chrome:// URL.
  NavigateToURL(shell(), chrome_top_url);

  // Inject a frame in the page and navigate it to a chrome:// URL as well.
  {
    std::string script = base::StringPrintf(
        "var frame = document.createElement('iframe');\n"
        "frame.src = '%s';\n"
        "document.body.appendChild(frame);\n",
        chrome_child_url.spec().c_str());

    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell(), script));
    navigation_observer.Wait();
    EXPECT_EQ(1U, root->child_count());

    // Ensure the subframe navigated to the expected URL and that it is in the
    // same SiteInstance as the parent frame.
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(1U, entry->root_node()->children.size());
    EXPECT_EQ(chrome_child_url,
              entry->root_node()->children[0]->frame_entry->url());
    EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
              root->child_at(0)->current_frame_host()->GetSiteInstance());
  }

  // Ensure that non-chrome:// pages get a different SiteInstance and process.
  {
    std::string script = base::StringPrintf(
        "var frame = document.createElement('iframe');\n"
        "frame.src = '%s';\n"
        "document.body.appendChild(frame);\n",
        regular_web_url.spec().c_str());

    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell(), script));
    navigation_observer.Wait();
    EXPECT_EQ(2U, root->child_count());

    // Ensure the subframe navigated to the expected URL and that it is in a
    // different SiteInstance from the parent frame.
    NavigationEntryImpl* entry = controller.GetLastCommittedEntry();
    ASSERT_EQ(2U, entry->root_node()->children.size());
    EXPECT_EQ(regular_web_url,
              entry->root_node()->children[1]->frame_entry->url());
    EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
              root->child_at(1)->current_frame_host()->GetSiteInstance());
  }
}

// Ensure that loading a page with cross-site coreferencing iframes does not
// cause an infinite number of nested iframes to be created.
// See https://crbug.com/650332.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest, CoReferencingFrames) {
  // Load a page with a cross-site coreferencing iframe. "Coreferencing" here
  // refers to two separate pages that contain subframes with URLs to each
  // other.
  StartEmbeddedServer();
  GURL url_1(
      embedded_test_server()->GetURL("a.com", "/coreferencingframe_1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // The FrameTree contains two successful instances of each site plus an
  // unsuccessfully-navigated third instance of B with a blank URL.  When not in
  // site-per-process mode, the FrameTreeVisualizer depicts all nodes as
  // referencing Site A because iframes are identified with their root site.
  if (AreAllSitesIsolatedForTesting()) {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "        +--Site A -- proxies for B\n"
        "             +--Site B -- proxies for A\n"
        "                  +--Site B -- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        FrameTreeVisualizer().DepictFrameTree(root));
  } else {
    EXPECT_EQ(
        " Site A\n"
        "   +--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "                  +--Site A\n"
        "Where A = http://a.com/",
        FrameTreeVisualizer().DepictFrameTree(root));
  }
  FrameTreeNode* bottom_child =
      root->child_at(0)->child_at(0)->child_at(0)->child_at(0);
  EXPECT_TRUE(bottom_child->current_url().is_empty());
  EXPECT_FALSE(bottom_child->has_committed_real_load());
}

// Ensures that nested subframes with the same URL but different fragments can
// only be nested once.  See https://crbug.com/650332.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingFragmentFrames) {
  StartEmbeddedServer();
  GURL url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html#123"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // ExecuteScript is used here and once more below because it is important to
  // use renderer-initiated navigations since browser-initiated navigations are
  // bypassed in the self-referencing navigation check.
  TestFrameNavigationObserver observer1(child);
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + url.spec() + "456" + "';"));
  observer1.Wait();

  FrameTreeNode* grandchild = child->child_at(0);
  GURL expected_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_EQ(expected_url, grandchild->current_url());

  // This navigation should be blocked.
  GURL blocked_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_iframe.html#123456789"));
  TestNavigationManager manager(web_contents, blocked_url);
  EXPECT_TRUE(ExecuteScript(grandchild,
                            "location.href = '" + blocked_url.spec() + "';"));
  // Wait for WillStartRequest and verify that the request is aborted before
  // starting it.
  EXPECT_FALSE(manager.WaitForRequestStart());
  WaitForLoadStop(web_contents);

  // The FrameTree contains two successful instances of the url plus an
  // unsuccessfully-navigated third instance with a blank URL.
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      FrameTreeVisualizer().DepictFrameTree(root));

  // The URL of the grandchild has not changed.
  EXPECT_EQ(expected_url, grandchild->current_url());
}

// Ensure that loading a page with a meta refresh iframe does not cause an
// infinite number of nested iframes to be created.  This test loads a page with
// an about:blank iframe where the page injects html containing a meta refresh
// into the iframe.  This test then checks that this does not cause infinite
// nested iframes to be created.  See https://crbug.com/527367.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingMetaRefreshFrames) {
  // Load a page with a blank iframe.
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL(
      "a.com", "/page_with_meta_refresh_frame.html"));
  NavigateToURLBlockUntilNavigationsComplete(shell(), url, 3);

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();

  // The third navigation should fail and be cancelled, leaving a FrameTree with
  // a height of 2.
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      FrameTreeVisualizer().DepictFrameTree(root));

  EXPECT_EQ(GURL(url::kAboutBlankURL),
            root->child_at(0)->child_at(0)->current_url());

  EXPECT_FALSE(root->child_at(0)->child_at(0)->has_committed_real_load());
}

// Ensure that navigating a subframe to the same URL as its parent twice in a
// row is not blocked by the self-reference check.
// See https://crbug.com/650332.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingSameURLRenavigation) {
  StartEmbeddedServer();
  GURL first_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  GURL second_url(first_url.spec() + "#123");
  EXPECT_TRUE(NavigateToURL(shell(), first_url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  TestFrameNavigationObserver observer1(child);
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + second_url.spec() + "';"));
  observer1.Wait();

  EXPECT_EQ(child->current_url(), second_url);

  TestFrameNavigationObserver observer2(child);
  // This navigation shouldn't be blocked. Blocking should only occur when more
  // than one ancestor has the same URL (excluding fragments), and the
  // navigating frame's current URL shouldn't count toward that.
  EXPECT_TRUE(
      ExecuteScript(child, "location.href = '" + first_url.spec() + "';"));
  observer2.Wait();

  EXPECT_EQ(child->current_url(), first_url);
}

// Ensures that POST requests bypass self-referential URL checks. See
// https://crbug.com/710008.
IN_PROC_BROWSER_TEST_F(RenderFrameHostManagerTest,
                       SelfReferencingFramesWithPOST) {
  StartEmbeddedServer();
  GURL url(embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  GURL child_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_EQ(url, root->current_url());
  EXPECT_EQ(child_url, child->current_url());

  // Navigate the child frame to the same URL as parent via POST.
  std::string script =
      "var f = document.createElement('form');\n"
      "f.method = 'POST';\n"
      "f.action = '/page_with_iframe.html';\n"
      "document.body.appendChild(f);\n"
      "f.submit();";
  {
    TestFrameNavigationObserver observer(child);
    EXPECT_TRUE(ExecuteScript(child, script));
    observer.Wait();
  }

  FrameTreeNode* grandchild = child->child_at(0);
  EXPECT_EQ(url, child->current_url());
  EXPECT_EQ(child_url, grandchild->current_url());

  // Now navigate the grandchild to the same URL as its two ancestors. This
  // should be allowed since it uses POST; it was blocked prior to
  // fixing https://crbug.com/710008.
  {
    TestFrameNavigationObserver observer(grandchild);
    EXPECT_TRUE(ExecuteScript(grandchild, script));
    observer.Wait();
  }

  EXPECT_EQ(url, grandchild->current_url());
  ASSERT_EQ(1U, grandchild->child_count());
  EXPECT_EQ(child_url, grandchild->child_at(0)->current_url());
}

}  // namespace content
