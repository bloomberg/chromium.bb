// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_process_policy.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/background_page_watcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

namespace {

void AddFrameToSet(std::set<content::RenderFrameHost*>* frames,
                   content::RenderFrameHost* rfh) {
  if (rfh->IsRenderFrameLive())
    frames->insert(rfh);
}

}  // namespace

// Takes a snapshot of all frames upon construction. When Wait() is called, a
// MessageLoop is created and Quit when all previously recorded frames are
// either present in the tab, or deleted. If a navigation happens between the
// construction and the Wait() call, then this logic ensures that all obsolete
// RenderFrameHosts have been destructed when Wait() returns.
// See also the comment at ProcessManagerBrowserTest::NavigateToURL.
class NavigationCompletedObserver : public content::WebContentsObserver {
 public:
  explicit NavigationCompletedObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {
    web_contents->ForEachFrame(
        base::Bind(&AddFrameToSet, base::Unretained(&frames_)));
  }

  void Wait() {
    if (!AreAllFramesInTab())
      message_loop_runner_->Run();
  }

  void RenderFrameDeleted(content::RenderFrameHost* rfh) override {
    if (frames_.erase(rfh) != 0 && message_loop_runner_->loop_running() &&
        AreAllFramesInTab())
      message_loop_runner_->Quit();
  }

 private:
  // Check whether all frames that were recorded at the construction of this
  // class are still part of the tab.
  bool AreAllFramesInTab() {
    std::set<content::RenderFrameHost*> current_frames;
    web_contents()->ForEachFrame(
        base::Bind(&AddFrameToSet, base::Unretained(&current_frames)));
    for (content::RenderFrameHost* frame : frames_) {
      if (current_frames.find(frame) == current_frames.end())
        return false;
    }
    return true;
  }

  std::set<content::RenderFrameHost*> frames_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(NavigationCompletedObserver);
};

// Exists as a browser test because ExtensionHosts are hard to create without
// a real browser.
class ProcessManagerBrowserTest : public ExtensionBrowserTest {
 public:
  // Create an extension with web-accessible frames and an optional background
  // page.
  const Extension* CreateExtension(const std::string& name,
                                   bool has_background_process) {
    scoped_ptr<TestExtensionDir> dir(new TestExtensionDir());

    DictionaryBuilder manifest;
    manifest.Set("name", name)
        .Set("version", "1")
        .Set("manifest_version", 2)
        // To allow ExecuteScript* to work.
        .Set("content_security_policy",
             "script-src 'self' 'unsafe-eval'; object-src 'self'")
        .Set("sandbox",
             std::move(DictionaryBuilder().Set(
                 "pages", std::move(ListBuilder().Append("sandboxed.html")))))
        .Set("web_accessible_resources", std::move(ListBuilder().Append("*")));

    if (has_background_process) {
      manifest.Set("background",
                   std::move(DictionaryBuilder().Set("page", "bg.html")));
      dir->WriteFile(FILE_PATH_LITERAL("bg.html"),
                     "<iframe id='bgframe' src='empty.html'></iframe>");
    }

    dir->WriteFile(FILE_PATH_LITERAL("blank_iframe.html"),
                   "<iframe id='frame0' src='about:blank'></iframe>");

    dir->WriteFile(FILE_PATH_LITERAL("srcdoc_iframe.html"),
                   "<iframe id='frame0' srcdoc='Hello world'></iframe>");

    dir->WriteFile(FILE_PATH_LITERAL("two_iframes.html"),
                   "<iframe id='frame1' src='empty.html'></iframe>"
                   "<iframe id='frame2' src='empty.html'></iframe>");

    dir->WriteFile(FILE_PATH_LITERAL("sandboxed.html"), "Some sandboxed page");

    dir->WriteFile(FILE_PATH_LITERAL("empty.html"), "");

    dir->WriteManifest(manifest.ToJSON());

    const Extension* extension = LoadExtension(dir->unpacked_path());
    EXPECT_TRUE(extension);
    temp_dirs_.push_back(dir.Pass());
    return extension;
  }

  // ui_test_utils::NavigateToURL sometimes returns too early: It returns as
  // soon as the StopLoading notification has been triggered. This does not
  // imply that RenderFrameDeleted was called, so the test may continue too
  // early and fail when ProcessManager::GetAllFrames() returns too many frames
  // (namely frames that are in the process of being deleted). To work around
  // this problem, we also wait until all previous frames have been deleted.
  void NavigateToURL(const GURL& url) {
    NavigationCompletedObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());

    ui_test_utils::NavigateToURL(browser(), url);

    // Wait until the last RenderFrameHosts are deleted. This wait doesn't take
    // long.
    observer.Wait();
  }

  size_t IfExtensionsIsolated(size_t if_enabled, size_t if_disabled) {
    return content::AreAllSitesIsolatedForTesting() ||
                   IsIsolateExtensionsEnabled()
               ? if_enabled
               : if_disabled;
  }

 private:
  std::vector<scoped_ptr<TestExtensionDir>> temp_dirs_;
};

// Test that basic extension loading creates the appropriate ExtensionHosts
// and background pages.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       ExtensionHostCreation) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllFrames().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));
  ASSERT_TRUE(extension.get());

  // Process manager gains a background host.
  EXPECT_EQ(1u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(extension.get()));

  // Unload the extension.
  UnloadExtension(extension->id());

  // Background host disappears.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(extension->id()));
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  EXPECT_FALSE(pm->IsBackgroundHostClosing(extension->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(extension.get()));
}

// Test that loading an extension with a browser action does not create a
// background page and that clicking on the action creates the appropriate
// ExtensionHost.
// Disabled due to flake, see http://crbug.com/315242
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest,
                       DISABLED_PopupHostCreation) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // Load an extension with the ability to open a popup but no background
  // page.
  scoped_refptr<const Extension> popup =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("popup"));
  ASSERT_TRUE(popup.get());

  // No background host was added.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(popup.get()));

  // Simulate clicking on the action to open a popup.
  BrowserActionTestUtil test_util(browser());
  content::WindowedNotificationObserver frame_observer(
      content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
      content::NotificationService::AllSources());
  // Open popup in the first extension.
  test_util.Press(0);
  frame_observer.Wait();
  ASSERT_TRUE(test_util.HasPopup());

  // We now have a view, but still no background hosts.
  EXPECT_EQ(0u, pm->background_hosts().size());
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_FALSE(pm->GetBackgroundHostForExtension(popup->id()));
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(popup->id()).size());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(popup->url()));
  EXPECT_FALSE(pm->IsBackgroundHostClosing(popup->id()));
  EXPECT_EQ(0, pm->GetLazyKeepaliveCount(popup.get()));
}

// Content loaded from http://hlogonemlfkgpejgnedahbkiabcdhnnn should not
// interact with an installed extension with that ID. Regression test
// for bug 357382.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, HttpHostMatchingExtensionId) {
  ProcessManager* pm = ProcessManager::Get(profile());

  // We start with no background hosts.
  ASSERT_EQ(0u, pm->background_hosts().size());
  ASSERT_EQ(0u, pm->GetAllFrames().size());

  // Load an extension with a background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("browser_action")
                        .AppendASCII("none"));

  // Set up a test server running at http://[extension-id]
  ASSERT_TRUE(extension.get());
  const std::string& aliased_host = extension->id();
  host_resolver()->AddRule(aliased_host, "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/extensions/test_file_with_body.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr(aliased_host);
  url = url.ReplaceComponents(replace_host);

  // Load a page from the test host in a new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      url,
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  // Sanity check that there's no bleeding between the extension and the tab.
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(url, tab_web_contents->GetVisibleURL());
  EXPECT_FALSE(pm->GetExtensionForWebContents(tab_web_contents))
      << "Non-extension content must not have an associated extension";
  ASSERT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
  content::WebContents* extension_web_contents =
      content::WebContents::FromRenderFrameHost(
          *pm->GetRenderFrameHostsForExtension(extension->id()).begin());
  EXPECT_TRUE(extension_web_contents->GetSiteInstance() !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetSiteInstanceForURL(extension->url()) !=
              tab_web_contents->GetSiteInstance());
  EXPECT_TRUE(pm->GetBackgroundHostForExtension(extension->id()));
}

IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, NoBackgroundPage) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ProcessManager* pm = ProcessManager::Get(profile());
  const Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("messaging")
                        .AppendASCII("connect_nobackground"));
  ASSERT_TRUE(extension);

  // The extension has no background page.
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  // Start in a non-extension process, then navigate to an extension process.
  NavigateToURL(embedded_test_server()->GetURL("/empty.html"));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  const GURL extension_url = extension->url().Resolve("manifest.json");
  NavigateToURL(extension_url);
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  NavigateToURL(GURL("about:blank"));
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension->id()).size());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), extension_url, NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension->id()).size());
}

// Tests whether frames are correctly classified. Non-extension frames should
// never appear in the list. Top-level extension frames should always appear.
// Child extension frames should only appear if it is hosted in an extension
// process (i.e. if the top-level frame is an extension page, or if OOP frames
// are enabled for extensions).
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, FrameClassification) {
  const Extension* extension1 = CreateExtension("Extension 1", false);
  const Extension* extension2 = CreateExtension("Extension 2", true);
  embedded_test_server()->ServeFilesFromDirectory(extension1->path());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL kExt1TwoFramesUrl(extension1->url().Resolve("two_iframes.html"));
  const GURL kExt1EmptyUrl(extension1->url().Resolve("empty.html"));
  const GURL kExt2TwoFramesUrl(extension2->url().Resolve("two_iframes.html"));
  const GURL kExt2EmptyUrl(extension2->url().Resolve("empty.html"));

  ProcessManager* pm = ProcessManager::Get(profile());

  // 1 background page + 1 frame in background page from Extension 2.
  BackgroundPageWatcher(pm, extension2).WaitForOpen();
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  ExecuteScriptInBackgroundPageNoWait(extension2->id(),
                                      "setTimeout(window.close, 0)");
  BackgroundPageWatcher(pm, extension2).WaitForClose();
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  NavigateToURL(embedded_test_server()->GetURL("/two_iframes.html"));
  EXPECT_EQ(0u, pm->GetAllFrames().size());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Tests extension frames in non-extension page.
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  EXPECT_EQ(IfExtensionsIsolated(1, 0),
            pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(IfExtensionsIsolated(1, 0), pm->GetAllFrames().size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame2", kExt2EmptyUrl));
  EXPECT_EQ(IfExtensionsIsolated(1, 0),
            pm->GetRenderFrameHostsForExtension(extension2->id()).size());
  EXPECT_EQ(IfExtensionsIsolated(2, 0), pm->GetAllFrames().size());

  // Tests non-extension page in extension frame.
  NavigateToURL(kExt1TwoFramesUrl);
  // 1 top-level + 2 child frames from Extension 1.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1",
                                           embedded_test_server()
                                           ->GetURL("/empty.html")));
  // 1 top-level + 1 child frame from Extension 1.
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(2u, pm->GetAllFrames().size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  // 1 top-level + 2 child frames from Extension 1.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());

  // Load a frame from another extension.
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt2EmptyUrl));
  // 1 top-level + 1 child frame from Extension 1,
  // 1 child frame from Extension 2.
  EXPECT_EQ(IfExtensionsIsolated(3, 2), pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(IfExtensionsIsolated(1, 0),
            pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Destroy all existing frames by navigating to another extension.
  NavigateToURL(extension2->url().Resolve("empty.html"));
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Test about:blank and about:srcdoc child frames.
  NavigateToURL(extension2->url().Resolve("srcdoc_iframe.html"));
  // 1 top-level frame + 1 child frame from Extension 2.
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  NavigateToURL(extension2->url().Resolve("blank_iframe.html"));
  // 1 top-level frame + 1 child frame from Extension 2.
  EXPECT_EQ(2u, pm->GetAllFrames().size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Sandboxed frames are not viewed as extension frames.
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame0",
                                           extension2->url()
                                           .Resolve("sandboxed.html")));
  // 1 top-level frame from Extension 2.
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  NavigateToURL(extension2->url().Resolve("sandboxed.html"));
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Test nested frames (same extension).
  NavigateToURL(kExt2TwoFramesUrl);
  // 1 top-level + 2 child frames from Extension 2.
  EXPECT_EQ(3u, pm->GetAllFrames().size());
  EXPECT_EQ(3u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt2TwoFramesUrl));
  // 1 top-level + 2 child frames from Extension 1,
  // 2 child frames in frame1 from Extension 2.
  EXPECT_EQ(5u, pm->GetAllFrames().size());
  EXPECT_EQ(5u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // The extension frame from the other extension should not be classified as an
  // extension (unless out-of-process frames are enabled).
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  // 1 top-level + 1 child frames from Extension 2,
  // 1 child frame from Extension 1.
  EXPECT_EQ(IfExtensionsIsolated(3, 2), pm->GetAllFrames().size());
  EXPECT_EQ(IfExtensionsIsolated(1, 0),
            pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(2u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame2", kExt1TwoFramesUrl));
  // 1 top-level + 1 child frames from Extension 2,
  // 1 child frame + 2 child frames in frame2 from Extension 1.
  EXPECT_EQ(IfExtensionsIsolated(5, 1), pm->GetAllFrames().size());
  EXPECT_EQ(IfExtensionsIsolated(4, 0),
            pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Crash tab where the top-level frame is an extension frame.
  content::CrashTab(tab);
  EXPECT_EQ(0u, pm->GetAllFrames().size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
  EXPECT_EQ(0u, pm->GetRenderFrameHostsForExtension(extension2->id()).size());

  // Now load an extension page and a non-extension page...
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), kExt1EmptyUrl, NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  NavigateToURL(embedded_test_server()->GetURL("/two_iframes.html"));
  EXPECT_EQ(1u, pm->GetAllFrames().size());

  // ... load an extension frame in the non-extension process
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "frame1", kExt1EmptyUrl));
  EXPECT_EQ(IfExtensionsIsolated(2, 1),
            pm->GetRenderFrameHostsForExtension(extension1->id()).size());

  // ... and take down the tab. The extension process is not part of the tab,
  // so it should be kept alive (minus the frames that died).
  content::CrashTab(tab);
  EXPECT_EQ(1u, pm->GetAllFrames().size());
  EXPECT_EQ(1u, pm->GetRenderFrameHostsForExtension(extension1->id()).size());
}

// Verify correct keepalive count behavior on network request events.
// Regression test for http://crbug.com/535716.
IN_PROC_BROWSER_TEST_F(ProcessManagerBrowserTest, KeepaliveOnNetworkRequest) {
  // Load an extension with a lazy background page.
  scoped_refptr<const Extension> extension =
      LoadExtension(test_data_dir_.AppendASCII("api_test")
                        .AppendASCII("lazy_background_page")
                        .AppendASCII("broadcast_event"));
  ASSERT_TRUE(extension.get());

  ProcessManager* pm = ProcessManager::Get(profile());
  ProcessManager::FrameSet frames =
      pm->GetRenderFrameHostsForExtension(extension->id());
  ASSERT_EQ(1u, frames.size());

  // Keepalive count at this point is unpredictable as there may be an
  // outstanding event dispatch. We use the current keepalive count as a
  // reliable baseline for future expectations.
  int baseline_keepalive = pm->GetLazyKeepaliveCount(extension.get());

  // Simulate some network events. This test assumes no other network requests
  // are pending, i.e., that there are no conflicts with the fake request IDs
  // we're using. This should be a safe assumption because LoadExtension should
  // wait for loads to complete, and we don't run the message loop otherwise.
  content::RenderFrameHost* frame_host = *frames.begin();
  pm->OnNetworkRequestStarted(frame_host, 1);
  EXPECT_EQ(baseline_keepalive + 1, pm->GetLazyKeepaliveCount(extension.get()));
  pm->OnNetworkRequestDone(frame_host, 1);
  EXPECT_EQ(baseline_keepalive, pm->GetLazyKeepaliveCount(extension.get()));

  // Simulate only a request completion for this ID and ensure it doesn't result
  // in keepalive decrement.
  pm->OnNetworkRequestDone(frame_host, 2);
  EXPECT_EQ(baseline_keepalive, pm->GetLazyKeepaliveCount(extension.get()));
}

}  // namespace extensions
