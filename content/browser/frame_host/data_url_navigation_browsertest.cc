// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/browser/site_per_process_browsertest.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#endif

namespace content {

namespace {

// The pattern to catch messages printed by the browser when a data URL
// navigation is blocked.
const char kDataUrlBlockedPattern[] =
    "Not allowed to navigate top frame to data URL:*";

// The message printed by the data URL when it successfully navigates.
const char kDataUrlSuccessfulMessage[] = "NAVIGATION_SUCCESSFUL";

// A "Hello World" PDF encoded as a data URL. Source of this PDF:
// -------------------------
// %PDF-1.7
// 1 0 obj << /Type /Page /Parent 3 0 R /Resources 5 0 R /Contents 2 0 R >>
// endobj
// 2 0 obj << /Length 51 >>
//  stream BT
//  /F1 12 Tf
//  1 0 0 1 100 20 Tm
//  (Hello World)Tj
//  ET
//  endstream
// endobj
// 3 0 obj << /Type /Pages /Kids [ 1 0 R ] /Count 1 /MediaBox [ 0 0 300 50] >>
// endobj
// 4 0 obj << /Type /Font /Subtype /Type1 /Name /F1 /BaseFont/Arial >>
// endobj
// 5 0 obj << /ProcSet[/PDF/Text] /Font <</F1 4 0 R >> >>
// endobj
// 6 0 obj << /Type /Catalog /Pages 3 0 R >>
// endobj
// trailer << /Root 6 0 R >>
// -------------------------
const char kPdfUrl[] =
    "data:application/pdf;base64,JVBERi0xLjcKMSAwIG9iaiA8PCAvVHlwZSAvUGFnZSAvUG"
    "FyZW50IDMgMCBSIC9SZXNvdXJjZXMgNSAwIFIgL0NvbnRlbnRzIDIgMCBSID4+CmVuZG9iagoy"
    "IDAgb2JqIDw8IC9MZW5ndGggNTEgPj4KIHN0cmVhbSBCVAogL0YxIDEyIFRmCiAxIDAgMCAxID"
    "EwMCAyMCBUbQogKEhlbGxvIFdvcmxkKVRqCiBFVAogZW5kc3RyZWFtCmVuZG9iagozIDAgb2Jq"
    "IDw8IC9UeXBlIC9QYWdlcyAvS2lkcyBbIDEgMCBSIF0gL0NvdW50IDEgL01lZGlhQm94IFsgMC"
    "AwIDMwMCA1MF0gPj4KZW5kb2JqCjQgMCBvYmogPDwgL1R5cGUgL0ZvbnQgL1N1YnR5cGUgL1R5"
    "cGUxIC9OYW1lIC9GMSAvQmFzZUZvbnQvQXJpYWwgPj4KZW5kb2JqCjUgMCBvYmogPDwgL1Byb2"
    "NTZXRbL1BERi9UZXh0XSAvRm9udCA8PC9GMSA0IDAgUiA+PiA+PgplbmRvYmoKNiAwIG9iaiA8"
    "PCAvVHlwZSAvQ2F0YWxvZyAvUGFnZXMgMyAwIFIgPj4KZW5kb2JqCnRyYWlsZXIgPDwgL1Jvb3"
    "QgNiAwIFIgPj4K";

enum ExpectedNavigationStatus { NAVIGATION_BLOCKED, NAVIGATION_ALLOWED };

// This class is similar to ConsoleObserverDelegate in that it listens and waits
// for specific console messages. The difference from ConsoleObserverDelegate is
// that this class immediately stops waiting if it sees a message matching
// fail_pattern, instead of waiting for a message matching success_pattern.
class DataURLWarningConsoleObserverDelegate : public WebContentsDelegate {
 public:
  DataURLWarningConsoleObserverDelegate(
      WebContents* web_contents,
      ExpectedNavigationStatus expected_navigation_status)
      : web_contents_(web_contents),
        success_filter_(expected_navigation_status == NAVIGATION_ALLOWED
                            ? kDataUrlSuccessfulMessage
                            : kDataUrlBlockedPattern),
        fail_filter_(expected_navigation_status == NAVIGATION_ALLOWED
                         ? kDataUrlBlockedPattern
                         : kDataUrlSuccessfulMessage),
        message_loop_runner_(
            new MessageLoopRunner(MessageLoopRunner::QuitMode::IMMEDIATE)),
        saw_failure_message_(false) {}
  ~DataURLWarningConsoleObserverDelegate() override {}

  void Wait() { message_loop_runner_->Run(); }

  // WebContentsDelegate method:
  bool DidAddMessageToConsole(WebContents* source,
                              int32_t level,
                              const base::string16& message,
                              int32_t line_no,
                              const base::string16& source_id) override {
    DCHECK(source == web_contents_);
    const std::string ascii_message = base::UTF16ToASCII(message);
    if (base::MatchPattern(ascii_message, fail_filter_)) {
      saw_failure_message_ = true;
      message_loop_runner_->Quit();
    }
    if (base::MatchPattern(ascii_message, success_filter_)) {
      message_loop_runner_->Quit();
    }
    return false;
  }

  // Returns true if the observer encountered a message that matches
  // |fail_filter_|.
  bool saw_failure_message() const { return saw_failure_message_; }

 private:
  WebContents* web_contents_;
  const std::string success_filter_;
  const std::string fail_filter_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  bool saw_failure_message_;
};

#if BUILDFLAG(ENABLE_PLUGINS)
// This class registers a fake PDF plugin handler so that data URL navigations
// with a PDF mime type end up with a navigation and don't simply download the
// file.
class ScopedPluginRegister {
 public:
  ScopedPluginRegister(content::PluginService* plugin_service)
      : plugin_service_(plugin_service) {
    const char kPluginName[] = "PDF";
    const char kPdfMimeType[] = "application/pdf";
    const char kPdfFileType[] = "pdf";
    WebPluginInfo plugin_info;
    plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;
    plugin_info.name = base::ASCIIToUTF16(kPluginName);
    plugin_info.mime_types.push_back(
        WebPluginMimeType(kPdfMimeType, kPdfFileType, std::string()));
    plugin_service_->RegisterInternalPlugin(plugin_info, false);
    plugin_service_->RefreshPlugins();
  }

  ~ScopedPluginRegister() {
    std::vector<WebPluginInfo> plugins;
    plugin_service_->GetInternalPlugins(&plugins);
    EXPECT_EQ(1u, plugins.size());
    plugin_service_->UnregisterInternalPlugin(plugins[0].path);
    plugin_service_->RefreshPlugins();

    plugins.clear();
    plugin_service_->GetInternalPlugins(&plugins);
    EXPECT_TRUE(plugins.empty());
  }

 private:
  content::PluginService* plugin_service_;
};
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}  // namespace

class DataUrlNavigationBrowserTest : public ContentBrowserTest {
 public:
#if BUILDFLAG(ENABLE_PLUGINS)
  DataUrlNavigationBrowserTest()
      : scoped_plugin_register_(PluginService::GetInstance()) {}
#else
  DataUrlNavigationBrowserTest() {}
#endif  // BUILDFLAG(ENABLE_PLUGINS)

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    base::FilePath path;
    ASSERT_TRUE(PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII("data_url_navigations.html");
    ASSERT_TRUE(base::PathExists(path));

    std::string contents;
    ASSERT_TRUE(base::ReadFileToString(path, &contents));
    data_url_ = GURL(std::string("data:text/html,") + contents);

    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());
    ShellDownloadManagerDelegate* delegate =
        static_cast<ShellDownloadManagerDelegate*>(
            shell()
                ->web_contents()
                ->GetBrowserContext()
                ->GetDownloadManagerDelegate());
    delegate->SetDownloadBehaviorForTesting(downloads_directory_.GetPath());
  }

  // Adds an iframe to |rfh| pointing to |url|.
  void AddIFrame(RenderFrameHost* rfh, const GURL& url) {
    const std::string javascript = base::StringPrintf(
        "f = document.createElement('iframe'); f.src = '%s';"
        "document.body.appendChild(f);",
        url.spec().c_str());
    TestNavigationObserver observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(rfh, javascript));
    observer.Wait();
  }

  // Runs |javascript| on the first child frame and checks for a navigation.
  void TestNavigationFromFrame(
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckNavigation(child, javascript,
                                    expected_navigation_status);
  }

  // Runs |javascript| on the first child frame and expects a download to occur.
  void TestDownloadFromFrame(const std::string& javascript) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckNavigationDownload(child, javascript);
  }

  // Runs |javascript| on the first child frame and checks for a navigation to
  // the PDF file pointed by the test case.
  void TestPDFNavigationFromFrame(
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
    ASSERT_TRUE(child);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckPDFNavigation(child, javascript,
                                       expected_navigation_status);
  }

  // Same as TestNavigationFromFrame, but instead of navigating, the child frame
  // tries to open a new window with a data URL.
  void TestWindowOpenFromFrame(
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    RenderFrameHost* child =
        ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
    if (AreAllSitesIsolatedForTesting()) {
      ASSERT_TRUE(child->IsCrossProcessSubframe());
    }
    ExecuteScriptAndCheckWindowOpen(child, javascript,
                                    expected_navigation_status);
  }

  // Executes |javascript| on |rfh| and waits for a console message based on
  // |expected_navigation_status|.
  // - Blocked navigations should print kDataUrlBlockedPattern.
  // - Allowed navigations should print kDataUrlSuccessfulMessage.
  void ExecuteScriptAndCheckNavigation(
      RenderFrameHost* rfh,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    const std::string expected_message;

    DataURLWarningConsoleObserverDelegate console_delegate(
        shell()->web_contents(), expected_navigation_status);
    shell()->web_contents()->SetDelegate(&console_delegate);

    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(rfh, javascript));
    console_delegate.Wait();
    EXPECT_FALSE(console_delegate.saw_failure_message());
    shell()->web_contents()->SetDelegate(nullptr);

    switch (expected_navigation_status) {
      case NAVIGATION_ALLOWED:
        navigation_observer.Wait();
        // The new page should have a data URL.
        EXPECT_TRUE(shell()->web_contents()->GetLastCommittedURL().SchemeIs(
            url::kDataScheme));
        EXPECT_TRUE(navigation_observer.last_navigation_url().SchemeIs(
            url::kDataScheme));
        EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
        break;

      case NAVIGATION_BLOCKED:
        // Original page shouldn't navigate away.
        EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
        EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
        break;

      default:
        NOTREACHED();
    }
  }

  // Similar to ExecuteScriptAndCheckNavigation(), but doesn't wait for a
  // console message if the navigation is expected to be allowed (this is
  // because PDF files can't print to the console).
  void ExecuteScriptAndCheckPDFNavigation(
      RenderFrameHost* rfh,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());

    const std::string expected_message =
        (expected_navigation_status == NAVIGATION_ALLOWED)
            ? std::string()
            : kDataUrlBlockedPattern;

    std::unique_ptr<ConsoleObserverDelegate> console_delegate;
    if (!expected_message.empty()) {
      console_delegate.reset(new ConsoleObserverDelegate(
          shell()->web_contents(), expected_message));
      shell()->web_contents()->SetDelegate(console_delegate.get());
    }

    TestNavigationObserver navigation_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(rfh, javascript));

    if (console_delegate) {
      console_delegate->Wait();
      shell()->web_contents()->SetDelegate(nullptr);
    }

    switch (expected_navigation_status) {
      case NAVIGATION_ALLOWED:
        navigation_observer.Wait();
        // The new page should have a data URL.
        EXPECT_TRUE(shell()->web_contents()->GetLastCommittedURL().SchemeIs(
            url::kDataScheme));
        EXPECT_TRUE(navigation_observer.last_navigation_url().SchemeIs(
            url::kDataScheme));
        EXPECT_TRUE(navigation_observer.last_navigation_succeeded());
        break;

      case NAVIGATION_BLOCKED:
        // Original page shouldn't navigate away.
        EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
        EXPECT_FALSE(navigation_observer.last_navigation_succeeded());
        break;

      default:
        NOTREACHED();
    }
  }

  // Executes |javascript| on |rfh| and waits for a new window to be opened.
  // Does not check for console messages (it's currently not possible to
  // concurrently wait for a new shell to be created and a console message to be
  // printed on that new shell).
  void ExecuteScriptAndCheckWindowOpen(
      RenderFrameHost* rfh,
      const std::string& javascript,
      ExpectedNavigationStatus expected_navigation_status) {
    ShellAddedObserver new_shell_observer;
    EXPECT_TRUE(ExecuteScript(rfh, javascript));

    Shell* new_shell = new_shell_observer.GetShell();
    WaitForLoadStop(new_shell->web_contents());

    switch (expected_navigation_status) {
      case NAVIGATION_ALLOWED:
        EXPECT_TRUE(new_shell->web_contents()->GetLastCommittedURL().SchemeIs(
            url::kDataScheme));
        break;

      case NAVIGATION_BLOCKED:
        EXPECT_TRUE(
            new_shell->web_contents()->GetLastCommittedURL().is_empty());
        break;

      default:
        NOTREACHED();
    }
  }

  // Executes |javascript| on |rfh| and waits for a download to be started by
  // a window.open call.
  void ExecuteScriptAndCheckWindowOpenDownload(RenderFrameHost* rfh,
                                               const std::string& javascript) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    ShellAddedObserver new_shell_observer;
    DownloadManager* download_manager = BrowserContext::GetDownloadManager(
        shell()->web_contents()->GetBrowserContext());

    EXPECT_TRUE(ExecuteScript(rfh, javascript));
    Shell* new_shell = new_shell_observer.GetShell();

    DownloadTestObserverTerminal download_observer(
        download_manager, 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

    WaitForLoadStop(new_shell->web_contents());
    // If no download happens, this will timeout.
    download_observer.WaitForFinished();

    EXPECT_TRUE(
        new_shell->web_contents()->GetLastCommittedURL().spec().empty());
    // No navigation should commit.
    EXPECT_FALSE(
        new_shell->web_contents()->GetController().GetLastCommittedEntry());
    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Executes |javascript| on |rfh| and waits for a download to be started.
  void ExecuteScriptAndCheckNavigationDownload(RenderFrameHost* rfh,
                                               const std::string& javascript) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    DownloadManager* download_manager = BrowserContext::GetDownloadManager(
        shell()->web_contents()->GetBrowserContext());
    DownloadTestObserverTerminal download_observer(
        download_manager, 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

    EXPECT_TRUE(ExecuteScript(rfh, javascript));
    // If no download happens, this will timeout.
    download_observer.WaitForFinished();

    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // Initiates a browser initiated navigation to |url| and waits for a download
  // to be started.
  void NavigateAndCheckDownload(const GURL& url) {
    const GURL original_url(shell()->web_contents()->GetLastCommittedURL());
    DownloadManager* download_manager = BrowserContext::GetDownloadManager(
        shell()->web_contents()->GetBrowserContext());
    DownloadTestObserverTerminal download_observer(
        download_manager, 1, DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
    NavigateToURL(shell(), url);

    // If no download happens, this will timeout.
    download_observer.WaitForFinished();

    // Original page shouldn't navigate away.
    EXPECT_EQ(original_url, shell()->web_contents()->GetLastCommittedURL());
  }

  // data URL form of the file at content/test/data/data_url_navigations.html
  GURL data_url() const { return data_url_; }

 private:
  base::ScopedTempDir downloads_directory_;

#if BUILDFLAG(ENABLE_PLUGINS)
  ScopedPluginRegister scoped_plugin_register_;
#endif  // BUILDFLAG(ENABLE_PLUGINS)

  GURL data_url_;

  DISALLOW_COPY_AND_ASSIGN(DataUrlNavigationBrowserTest);
};

////////////////////////////////////////////////////////////////////////////////
// data URLs with HTML mimetype
//
// Tests that a browser initiated navigation to a data URL doesn't show a
// console warning and is not blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, BrowserInitiated_Allow) {
  DataURLWarningConsoleObserverDelegate console_delegate(
      shell()->web_contents(), NAVIGATION_ALLOWED);
  shell()->web_contents()->SetDelegate(&console_delegate);

  NavigateToURL(shell(), GURL("data:text/"
                              "html,<html><script>console.log('NAVIGATION_"
                              "SUCCESSFUL');</script>"));
  console_delegate.Wait();
  shell()->web_contents()->SetDelegate(nullptr);

  EXPECT_TRUE(shell()->web_contents()->GetLastCommittedURL().SchemeIs(
      url::kDataScheme));
}

// Tests that a content initiated navigation to a data URL is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, HTML_Navigation_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that a content initiated navigation to a data URL is allowed if
// blocking is disabled with a feature flag.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_Navigation_Allow_FeatureFlag) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kAllowContentInitiatedDataUrlNavigations);
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_ALLOWED);
}

// Tests that a window.open to a data URL with HTML mime type is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, HTML_WindowOpen_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckWindowOpen(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('window-open-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that a form post to a data URL with HTML mime type is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, HTML_FormPost_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('form-post-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that navigating the main frame to a data URL with HTML mimetype from a
// subframe is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_NavigationFromFrame_Block) {
  // This test fails and is disabled in site-per-process + no plznavigate mode.
  // request->originDocument is null in FrameLoader::prepareForRequest,
  // allowing the navigation by default. See https://crbug.com/647839
  if (AreAllSitesIsolatedForTesting() && !IsBrowserSideNavigationEnabled()) {
    return;
  }

  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  AddIFrame(
      shell()->web_contents()->GetMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

  TestNavigationFromFrame(
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that opening a new data URL window from a subframe is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_WindowOpenFromFrame_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  AddIFrame(
      shell()->web_contents()->GetMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

  TestWindowOpenFromFrame("document.getElementById('window-open-html').click()",
                          NAVIGATION_BLOCKED);
}

// Tests that navigation to a data URL is blocked even if the top frame is
// already a data URL.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_Navigation_DataToData_Block) {
  NavigateToURL(shell(), data_url());
  ExecuteScriptAndCheckNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that a form post to a data URL with HTML mime type is blocked even if
// the top frame is already a data URL.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_FormPost_DataToData_Block) {
  NavigateToURL(shell(), data_url());
  ExecuteScriptAndCheckNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('form-post-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that navigating the top frame to a data URL with HTML mimetype is
// blocked even if the top frame is already a data URL.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_NavigationFromFrame_TopFrameIsDataURL_Block) {
  // This test fails and is disabled in site-per-process + no plznavigate mode.
  // request->originDocument is null in FrameLoader::prepareForRequest,
  // allowing the navigation by default. See https://crbug.com/647839
  if (AreAllSitesIsolatedForTesting() && !IsBrowserSideNavigationEnabled()) {
    return;
  }

  const GURL top_url(
      base::StringPrintf("data:text/html, <iframe src='%s'></iframe>",
                         embedded_test_server()
                             ->GetURL("/data_url_navigations.html")
                             .spec()
                             .c_str()));
  NavigateToURL(shell(), top_url);

  TestNavigationFromFrame(
      "document.getElementById('navigate-top-frame-to-html').click()",
      NAVIGATION_BLOCKED);
}

// Tests that opening a new window with a data URL with HTML mimetype is blocked
// even if the top frame is already a data URL.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       HTML_WindowOpenFromFrame_TopFrameIsDataURL_Block) {
  const GURL top_url(
      base::StringPrintf("data:text/html, <iframe src='%s'></iframe>",
                         embedded_test_server()
                             ->GetURL("/data_url_navigations.html")
                             .spec()
                             .c_str()));
  NavigateToURL(shell(), top_url);

  TestWindowOpenFromFrame("document.getElementById('window-open-html').click()",
                          NAVIGATION_BLOCKED);
}

////////////////////////////////////////////////////////////////////////////////
// data URLs with octet-stream mimetype (binary)
//
// Test that a direct navigation to a binary mime type initiates a download.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       OctetStream_BrowserInitiated_Download) {
  NavigateAndCheckDownload(GURL("data:application/octet-stream,test"));
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/734563
#define MAYBE_OctetStream_WindowOpen_Download \
  DISABLED_OctetStream_WindowOpen_Download
#else
#define MAYBE_OctetStream_WindowOpen_Download OctetStream_WindowOpen_Download
#endif

// Test that window.open to a data URL results in a download if the URL has a
// binary mime type.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       MAYBE_OctetStream_WindowOpen_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckWindowOpenDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('window-open-octetstream').click()");
}

// Test that a navigation to a data URL results in a download if the URL has a
// binary mime type.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       OctetStream_Navigation_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-frame-to-octetstream').click()");
}

// Test that a form post to a data URL results in a download if the URL has a
// binary mime type.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       OctetStream_FormPost_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('form-post-to-octetstream').click()");
}

// Tests that navigating the main frame from a subframe results in a download
// if the URL has a binary mimetype.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       OctetStream_NavigationFromFrame_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  AddIFrame(
      shell()->web_contents()->GetMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

  TestDownloadFromFrame(
      "document.getElementById('navigate-top-frame-to-octetstream').click()");
}

////////////////////////////////////////////////////////////////////////////////
// data URLs with unknown mimetype
//
// Test that a direct navigation to an unknown mime type initiates a download.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       UnknownMimeType_BrowserInitiated_Download) {
  NavigateAndCheckDownload(GURL("data:unknown/mimetype,test"));
}

#if defined(OS_ANDROID)
// Flaky on android: https://crbug.com/734563
#define MAYBE_UnknownMimeType_WindowOpen_Download \
  DISABLED_UnknownMimeType_WindowOpen_Download
#else
#define MAYBE_UnknownMimeType_WindowOpen_Download \
  UnknownMimeType_WindowOpen_Download
#endif

// Test that window.open to a data URL results in a download if the URL has an
// unknown mime type.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       MAYBE_UnknownMimeType_WindowOpen_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckWindowOpenDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('window-open-unknown-mimetype').click()");
}

// Test that a navigation to a data URL results in a download if the URL has an
// unknown mime type.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       UnknownMimeType_Navigation_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-"
      "frame-to-unknown-mimetype').click()");
}

// Test that a form post to a data URL results in a download if the URL has an
// unknown mime type.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       UnknownMimeType_FormPost_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('form-post-to-unknown-mimetype').click()");
}

// Tests that navigating the main frame from a subframe results in a download
// if the URL has an unknown mimetype.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       UnknownMimeType_NavigationFromFrame_Download) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  AddIFrame(
      shell()->web_contents()->GetMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

  TestDownloadFromFrame(
      "document.getElementById('navigate-top-frame-to-unknown-mimetype').click("
      ")");
}

////////////////////////////////////////////////////////////////////////////////
// data URLs with PDF mimetype
//
// Tests that a browser initiated navigation to a data URL with PDF mime type is
// allowed, or initiates a download on Android.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       PDF_BrowserInitiatedNavigation_Allow) {
#if !defined(OS_ANDROID)
  TestNavigationObserver observer(shell()->web_contents());
  NavigateToURL(shell(), GURL(kPdfUrl));
  EXPECT_EQ(GURL(kPdfUrl), observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_TRUE(shell()->web_contents()->GetLastCommittedURL().SchemeIs(
      url::kDataScheme));
#else
  // On Android, PDFs are downloaded upon navigation.
  NavigateAndCheckDownload(GURL(kPdfUrl));
#endif
}

// Tests that a window.open to a data URL is blocked if the data URL has a
// mime type that will be handled by a plugin (PDF in this case).
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, PDF_WindowOpen_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));

#if !defined(OS_ANDROID)
  ExecuteScriptAndCheckWindowOpen(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('window-open-pdf').click()", NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('window-open-pdf').click()");
#endif
}

// Test that a navigation to a data URL is blocked if the data URL has a mime
// type that will be handled by a plugin (PDF in this case).
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, PDF_Navigation_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));

#if !defined(OS_ANDROID)
  ExecuteScriptAndCheckPDFNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-frame-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('navigate-top-frame-to-pdf').click()");
#endif
}

// Test that a form post to a data URL is blocked if the data URL has a mime
// type that will be handled by a plugin (PDF in this case).
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest, PDF_FormPost_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));

#if !defined(OS_ANDROID)
  ExecuteScriptAndCheckPDFNavigation(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('form-post-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  ExecuteScriptAndCheckNavigationDownload(
      shell()->web_contents()->GetMainFrame(),
      "document.getElementById('form-post-to-pdf').click()");
#endif
}

// Tests that navigating the main frame to a data URL with PDF mimetype from a
// subframe is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       PDF_NavigationFromFrame_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  AddIFrame(
      shell()->web_contents()->GetMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

#if !defined(OS_ANDROID)
  TestPDFNavigationFromFrame(
      "document.getElementById('navigate-top-frame-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  RenderFrameHost* child =
      ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  if (AreAllSitesIsolatedForTesting()) {
    ASSERT_TRUE(child->IsCrossProcessSubframe());
  }
  ExecuteScriptAndCheckNavigationDownload(
      child, "document.getElementById('navigate-top-frame-to-pdf').click()");
#endif
}

// Tests that opening a window with a data URL with PDF mimetype from a
// subframe is blocked.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       PDF_WindowOpenFromFrame_Block) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("a.com", "/simple_page.html"));
  AddIFrame(
      shell()->web_contents()->GetMainFrame(),
      embedded_test_server()->GetURL("b.com", "/data_url_navigations.html"));

#if !defined(OS_ANDROID)
  TestWindowOpenFromFrame("document.getElementById('window-open-pdf').click()",
                          NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  RenderFrameHost* child =
      ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  if (AreAllSitesIsolatedForTesting()) {
    ASSERT_TRUE(child->IsCrossProcessSubframe());
  }
  ExecuteScriptAndCheckNavigationDownload(
      child, "document.getElementById('window-open-pdf').click()");
#endif
}

// Tests that navigating the top frame to a data URL with PDF mimetype from a
// subframe is blocked even if the top frame is already a data URL.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       PDF_NavigationFromFrame_TopFrameIsDataURL_Block) {
  const GURL top_url(
      base::StringPrintf("data:text/html, <iframe src='%s'></iframe>",
                         embedded_test_server()
                             ->GetURL("/data_url_navigations.html")
                             .spec()
                             .c_str()));
  NavigateToURL(shell(), top_url);

#if !defined(OS_ANDROID)
  TestPDFNavigationFromFrame(
      "document.getElementById('navigate-top-frame-to-pdf').click()",
      NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  RenderFrameHost* child =
      ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  if (AreAllSitesIsolatedForTesting()) {
    ASSERT_TRUE(child->IsCrossProcessSubframe());
  }
  ExecuteScriptAndCheckNavigationDownload(
      child, "document.getElementById('navigate-top-frame-to-pdf').click()");
#endif
}

// Tests that opening a window with a data URL with PDF mimetype from a
// subframe is blocked even if the top frame is already a data URL.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       PDF_WindowOpenFromFrame_TopFrameIsDataURL_Block) {
  const GURL top_url(
      base::StringPrintf("data:text/html, <iframe src='%s'></iframe>",
                         embedded_test_server()
                             ->GetURL("/data_url_navigations.html")
                             .spec()
                             .c_str()));
  NavigateToURL(shell(), top_url);

#if !defined(OS_ANDROID)
  TestWindowOpenFromFrame("document.getElementById('window-open-pdf').click()",
                          NAVIGATION_BLOCKED);
#else
  // On Android, PDFs are downloaded upon navigation.
  RenderFrameHost* child =
      ChildFrameAt(shell()->web_contents()->GetMainFrame(), 0);
  ASSERT_TRUE(child);
  if (AreAllSitesIsolatedForTesting()) {
    ASSERT_TRUE(child->IsCrossProcessSubframe());
  }
  ExecuteScriptAndCheckNavigationDownload(
      child, "document.getElementById('window-open-pdf').click()");
#endif
}

// Test case to verify that redirects to data: URLs are properly disallowed,
// even when invoked through history navigations.
// See https://crbug.com/723796.
IN_PROC_BROWSER_TEST_F(DataUrlNavigationBrowserTest,
                       WindowOpenRedirectAndBack) {
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/data_url_navigations.html"));

  // This test will need to navigate the newly opened window.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(),
                    "document.getElementById('window-open-redirect').click()"));
  Shell* new_shell = new_shell_observer.GetShell();
  NavigationController* controller =
      &new_shell->web_contents()->GetController();
  WaitForLoadStop(new_shell->web_contents());

  // The window.open() should have resulted in an error page. The blocked
  // URL should be in the virtual URL, not the actual URL.
  //
  // TODO(nasko): Now that the error commits on the previous URL, the blocked
  // navigation logic is no longer needed. https://crbug.com/723796
  {
    EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
    NavigationEntry* entry = controller->GetLastCommittedEntry();
    EXPECT_EQ(PAGE_TYPE_ERROR, entry->GetPageType());
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kDataScheme));
    EXPECT_TRUE(base::StartsWith(
        entry->GetVirtualURL().spec(),
        embedded_test_server()->GetURL("/server-redirect?").spec(),
        base::CompareCase::SENSITIVE));
  }

  // Navigate forward and then go back to ensure the navigation to data: URL
  // is blocked. Use a browser-initiated back navigation, equivalent to user
  // pressing the back button.
  EXPECT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(1, controller->GetLastCommittedEntryIndex());
  {
    TestNavigationObserver observer(new_shell->web_contents());
    controller->GoBack();
    observer.Wait();

    NavigationEntry* entry = controller->GetLastCommittedEntry();
    EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kDataScheme));
    EXPECT_TRUE(base::StartsWith(
        entry->GetVirtualURL().spec(),
        embedded_test_server()->GetURL("/server-redirect?").spec(),
        base::CompareCase::SENSITIVE));
    EXPECT_EQ(url::kAboutBlankURL, entry->GetURL().spec());
  }

  // Do another new navigation, but then use JavaScript to navigate back,
  // equivalent to document executing JS.
  EXPECT_TRUE(
      NavigateToURL(new_shell, embedded_test_server()->GetURL("/title1.html")));
  EXPECT_EQ(1, controller->GetLastCommittedEntryIndex());
  {
    TestNavigationObserver observer(new_shell->web_contents());
    EXPECT_TRUE(ExecuteScript(new_shell, "history.go(-1)"));
    observer.Wait();

    NavigationEntry* entry = controller->GetLastCommittedEntry();
    EXPECT_EQ(0, controller->GetLastCommittedEntryIndex());
    EXPECT_FALSE(entry->GetURL().SchemeIs(url::kDataScheme));
    EXPECT_TRUE(base::StartsWith(
        entry->GetVirtualURL().spec(),
        embedded_test_server()->GetURL("/server-redirect?").spec(),
        base::CompareCase::SENSITIVE));
    EXPECT_EQ(url::kAboutBlankURL, entry->GetURL().spec());
  }
}

}  // content
