// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/browser/guest_view/web_view/test_guest_view_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#endif

// For fine-grained suppression on flaky tests.
#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using extensions::ContextMenuMatcher;
using extensions::MenuItem;
using prerender::PrerenderLinkManager;
using prerender::PrerenderLinkManagerFactory;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyBackground;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchAnyWebView;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchBackground;
using task_manager::browsertest_util::MatchWebView;
using task_manager::browsertest_util::WaitForTaskManagerRows;
using ui::MenuModel;

namespace {
const char kEmptyResponsePath[] = "/close-socket";
const char kRedirectResponsePath[] = "/server-redirect";
const char kRedirectResponseFullPath[] =
    "/extensions/platform_apps/web_view/shim/guest_redirect.html";

// Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
const wchar_t library_name[] = L"ppapi_tests.dll";
#elif defined(OS_MACOSX)
const char library_name[] = "ppapi_tests.plugin";
#elif defined(OS_POSIX)
const char library_name[] = "libppapi_tests.so";
#endif

class EmptyHttpResponse : public net::test_server::HttpResponse {
 public:
  virtual std::string ToResponseString() const OVERRIDE {
    return std::string();
  }
};

class TestInterstitialPageDelegate : public content::InterstitialPageDelegate {
 public:
  TestInterstitialPageDelegate() {
  }
  virtual ~TestInterstitialPageDelegate() {}
  virtual std::string GetHTMLContents() OVERRIDE { return std::string(); }
};

class WebContentsHiddenObserver : public content::WebContentsObserver {
 public:
  WebContentsHiddenObserver(content::WebContents* web_contents,
                            const base::Closure& hidden_callback)
      : WebContentsObserver(web_contents),
        hidden_callback_(hidden_callback),
        hidden_observed_(false) {
  }

  // WebContentsObserver.
  virtual void WasHidden() OVERRIDE {
    hidden_observed_ = true;
    hidden_callback_.Run();
  }

  bool hidden_observed() { return hidden_observed_; }

 private:
  base::Closure hidden_callback_;
  bool hidden_observed_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsHiddenObserver);
};

class InterstitialObserver : public content::WebContentsObserver {
 public:
  InterstitialObserver(content::WebContents* web_contents,
                       const base::Closure& attach_callback,
                       const base::Closure& detach_callback)
      : WebContentsObserver(web_contents),
        attach_callback_(attach_callback),
        detach_callback_(detach_callback) {
  }

  virtual void DidAttachInterstitialPage() OVERRIDE {
    attach_callback_.Run();
  }

  virtual void DidDetachInterstitialPage() OVERRIDE {
    detach_callback_.Run();
  }

 private:
  base::Closure attach_callback_;
  base::Closure detach_callback_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

void ExecuteScriptWaitForTitle(content::WebContents* web_contents,
                               const char* script,
                               const char* title) {
  base::string16 expected_title(base::ASCIIToUTF16(title));
  base::string16 error_title(base::ASCIIToUTF16("error"));

  content::TitleWatcher title_watcher(web_contents, expected_title);
  title_watcher.AlsoWaitForTitle(error_title);
  EXPECT_TRUE(content::ExecuteScript(web_contents, script));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

}  // namespace

// This class intercepts media access request from the embedder. The request
// should be triggered only if the embedder API (from tests) allows the request
// in Javascript.
// We do not issue the actual media request; the fact that the request reached
// embedder's WebContents is good enough for our tests. This is also to make
// the test run successfully on trybots.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() : requested_(false) {}
  virtual ~MockWebContentsDelegate() {}

  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {
    requested_ = true;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  void WaitForSetMediaPermission() {
    if (requested_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  bool requested_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockWebContentsDelegate);
};

// This class intercepts download request from the guest.
class MockDownloadWebContentsDelegate : public content::WebContentsDelegate {
 public:
  explicit MockDownloadWebContentsDelegate(
      content::WebContentsDelegate* orig_delegate)
      : orig_delegate_(orig_delegate),
        waiting_for_decision_(false),
        expect_allow_(false),
        decision_made_(false),
        last_download_allowed_(false) {}
  virtual ~MockDownloadWebContentsDelegate() {}

  virtual void CanDownload(
      content::RenderViewHost* render_view_host,
      const GURL& url,
      const std::string& request_method,
      const base::Callback<void(bool)>& callback) OVERRIDE {
    orig_delegate_->CanDownload(
        render_view_host, url, request_method,
        base::Bind(&MockDownloadWebContentsDelegate::DownloadDecided,
                   base::Unretained(this)));
  }

  void WaitForCanDownload(bool expect_allow) {
    EXPECT_FALSE(waiting_for_decision_);
    waiting_for_decision_ = true;

    if (decision_made_) {
      EXPECT_EQ(expect_allow, last_download_allowed_);
      return;
    }

    expect_allow_ = expect_allow;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  void DownloadDecided(bool allow) {
    EXPECT_FALSE(decision_made_);
    decision_made_ = true;

    if (waiting_for_decision_) {
      EXPECT_EQ(expect_allow_, allow);
      if (message_loop_runner_.get())
        message_loop_runner_->Quit();
      return;
    }
    last_download_allowed_ = allow;
  }

  void Reset() {
    waiting_for_decision_ = false;
    decision_made_ = false;
  }

 private:
  content::WebContentsDelegate* orig_delegate_;
  bool waiting_for_decision_;
  bool expect_allow_;
  bool decision_made_;
  bool last_download_allowed_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockDownloadWebContentsDelegate);
};

class WebViewTest : public extensions::PlatformAppBrowserTest {
 protected:
  virtual void SetUp() OVERRIDE {
    if (UsesFakeSpeech()) {
      // SpeechRecognition test specific SetUp.
      fake_speech_recognition_manager_.reset(
          new content::FakeSpeechRecognitionManager());
      fake_speech_recognition_manager_->set_should_send_fake_response(true);
      // Inject the fake manager factory so that the test result is returned to
      // the web page.
      content::SpeechRecognitionManager::SetManagerForTesting(
          fake_speech_recognition_manager_.get());
    }
    extensions::PlatformAppBrowserTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    if (UsesFakeSpeech()) {
      // SpeechRecognition test specific TearDown.
      content::SpeechRecognitionManager::SetManagerForTesting(NULL);
    }

    extensions::PlatformAppBrowserTest::TearDown();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // Mock out geolocation for geolocation specific tests.
    if (!strncmp(test_info->name(), "GeolocationAPI",
            strlen("GeolocationAPI"))) {
      ui_test_utils::OverrideGeolocation(10, 20);
    }
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");

    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  // This method is responsible for initializing a packaged app, which contains
  // multiple webview tags. The tags have different partition identifiers and
  // their WebContent objects are returned as output. The method also verifies
  // the expected process allocation and storage partition assignment.
  // The |navigate_to_url| parameter is used to navigate the main browser
  // window.
  //
  // TODO(ajwong): This function is getting to be too large. Either refactor it
  // so the test can specify a configuration of WebView tags that we will
  // dynamically inject JS to generate, or move this test wholesale into
  // something that RunPlatformAppTest() can execute purely in Javascript. This
  // won't let us do a white-box examination of the StoragePartition equivalence
  // directly, but we will be able to view the black box effects which is good
  // enough.  http://crbug.com/160361
  void NavigateAndOpenAppForIsolation(
      GURL navigate_to_url,
      content::WebContents** default_tag_contents1,
      content::WebContents** default_tag_contents2,
      content::WebContents** named_partition_contents1,
      content::WebContents** named_partition_contents2,
      content::WebContents** persistent_partition_contents1,
      content::WebContents** persistent_partition_contents2,
      content::WebContents** persistent_partition_contents3) {
    GURL::Replacements replace_host;
    std::string host_str("localhost");  // Must stay in scope with replace_host.
    replace_host.SetHostStr(host_str);

    navigate_to_url = navigate_to_url.ReplaceComponents(replace_host);

    GURL tag_url1 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/cookie.html");
    tag_url1 = tag_url1.ReplaceComponents(replace_host);
    GURL tag_url2 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/cookie2.html");
    tag_url2 = tag_url2.ReplaceComponents(replace_host);
    GURL tag_url3 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/storage1.html");
    tag_url3 = tag_url3.ReplaceComponents(replace_host);
    GURL tag_url4 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/storage2.html");
    tag_url4 = tag_url4.ReplaceComponents(replace_host);
    GURL tag_url5 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/storage1.html#p1");
    tag_url5 = tag_url5.ReplaceComponents(replace_host);
    GURL tag_url6 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/storage1.html#p2");
    tag_url6 = tag_url6.ReplaceComponents(replace_host);
    GURL tag_url7 = embedded_test_server()->GetURL(
        "/extensions/platform_apps/web_view/isolation/storage1.html#p3");
    tag_url7 = tag_url7.ReplaceComponents(replace_host);

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), navigate_to_url, CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    ui_test_utils::UrlLoadObserver observer1(
        tag_url1, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer2(
        tag_url2, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer3(
        tag_url3, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer4(
        tag_url4, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer5(
        tag_url5, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer6(
        tag_url6, content::NotificationService::AllSources());
    ui_test_utils::UrlLoadObserver observer7(
        tag_url7, content::NotificationService::AllSources());
    LoadAndLaunchPlatformApp("web_view/isolation", "Launched");
    observer1.Wait();
    observer2.Wait();
    observer3.Wait();
    observer4.Wait();
    observer5.Wait();
    observer6.Wait();
    observer7.Wait();

    content::Source<content::NavigationController> source1 = observer1.source();
    EXPECT_TRUE(source1->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());
    content::Source<content::NavigationController> source2 = observer2.source();
    EXPECT_TRUE(source2->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());
    content::Source<content::NavigationController> source3 = observer3.source();
    EXPECT_TRUE(source3->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());
    content::Source<content::NavigationController> source4 = observer4.source();
    EXPECT_TRUE(source4->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());
    content::Source<content::NavigationController> source5 = observer5.source();
    EXPECT_TRUE(source5->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());
    content::Source<content::NavigationController> source6 = observer6.source();
    EXPECT_TRUE(source6->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());
    content::Source<content::NavigationController> source7 = observer7.source();
    EXPECT_TRUE(source7->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());

    // Check that the first two tags use the same process and it is different
    // than the process used by the other two.
    EXPECT_EQ(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              source2->GetWebContents()->GetRenderProcessHost()->GetID());
    EXPECT_EQ(source3->GetWebContents()->GetRenderProcessHost()->GetID(),
              source4->GetWebContents()->GetRenderProcessHost()->GetID());
    EXPECT_NE(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              source3->GetWebContents()->GetRenderProcessHost()->GetID());

    // The two sets of tags should also be isolated from the main browser.
    EXPECT_NE(source1->GetWebContents()->GetRenderProcessHost()->GetID(),
              browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetRenderProcessHost()->GetID());
    EXPECT_NE(source3->GetWebContents()->GetRenderProcessHost()->GetID(),
              browser()->tab_strip_model()->GetWebContentsAt(0)->
                  GetRenderProcessHost()->GetID());

    // Check that the storage partitions of the first two tags match and are
    // different than the other two.
    EXPECT_EQ(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source2->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_EQ(
        source3->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source4->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source3->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());

    // Ensure the persistent storage partitions are different.
    EXPECT_EQ(
        source5->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source6->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source5->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source7->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source5->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());
    EXPECT_NE(
        source1->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition(),
        source7->GetWebContents()->GetRenderProcessHost()->
            GetStoragePartition());

    *default_tag_contents1 = source1->GetWebContents();
    *default_tag_contents2 = source2->GetWebContents();
    *named_partition_contents1 = source3->GetWebContents();
    *named_partition_contents2 = source4->GetWebContents();
    if (persistent_partition_contents1) {
      *persistent_partition_contents1 = source5->GetWebContents();
    }
    if (persistent_partition_contents2) {
      *persistent_partition_contents2 = source6->GetWebContents();
    }
    if (persistent_partition_contents3) {
      *persistent_partition_contents3 = source7->GetWebContents();
    }
  }

  // Handles |request| by serving a redirect response.
  static scoped_ptr<net::test_server::HttpResponse> RedirectResponseHandler(
      const std::string& path,
      const GURL& redirect_target,
      const net::test_server::HttpRequest& request) {
    if (!StartsWithASCII(path, request.relative_url, true))
      return scoped_ptr<net::test_server::HttpResponse>();

    scoped_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_target.spec());
    return http_response.PassAs<net::test_server::HttpResponse>();
  }

  // Handles |request| by serving an empty response.
  static scoped_ptr<net::test_server::HttpResponse> EmptyResponseHandler(
      const std::string& path,
      const net::test_server::HttpRequest& request) {
    if (StartsWithASCII(path, request.relative_url, true)) {
      return scoped_ptr<net::test_server::HttpResponse>(
          new EmptyHttpResponse);
    }

    return scoped_ptr<net::test_server::HttpResponse>();
  }

  // Shortcut to return the current MenuManager.
  extensions::MenuManager* menu_manager() {
    return extensions::MenuManager::Get(browser()->profile());
  }

  // This gets all the items that any extension has registered for possible
  // inclusion in context menus.
  MenuItem::List GetItems() {
    MenuItem::List result;
    std::set<MenuItem::ExtensionKey> extension_ids =
        menu_manager()->ExtensionIds();
    std::set<MenuItem::ExtensionKey>::iterator i;
    for (i = extension_ids.begin(); i != extension_ids.end(); ++i) {
      const MenuItem::List* list = menu_manager()->MenuItems(*i);
      result.insert(result.end(), list->begin(), list->end());
    }
    return result;
  }

  enum TestServer {
    NEEDS_TEST_SERVER,
    NO_TEST_SERVER
  };

  void TestHelper(const std::string& test_name,
                  const std::string& app_location,
                  TestServer test_server) {
    // For serving guest pages.
    if (test_server == NEEDS_TEST_SERVER) {
      if (!StartEmbeddedTestServer()) {
        LOG(ERROR) << "FAILED TO START TEST SERVER.";
        return;
      }
      embedded_test_server()->RegisterRequestHandler(
          base::Bind(&WebViewTest::RedirectResponseHandler,
                    kRedirectResponsePath,
                    embedded_test_server()->GetURL(kRedirectResponseFullPath)));

      embedded_test_server()->RegisterRequestHandler(
          base::Bind(&WebViewTest::EmptyResponseHandler, kEmptyResponsePath));
    }

    LoadAndLaunchPlatformApp(app_location.c_str(), "Launched");

    // Flush any pending events to make sure we start with a clean slate.
    content::RunAllPendingInMessageLoop();

    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    if (!embedder_web_contents) {
      LOG(ERROR) << "UNABLE TO FIND EMBEDDER WEB CONTENTS.";
      return;
    }

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            embedder_web_contents,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "UNABLE TO START TEST.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  content::WebContents* LoadGuest(const std::string& guest_path,
                                  const std::string& app_path) {
    GURL::Replacements replace_host;
    std::string host_str("localhost");  // Must stay in scope with replace_host.
    replace_host.SetHostStr(host_str);

    GURL guest_url = embedded_test_server()->GetURL(guest_path);
    guest_url = guest_url.ReplaceComponents(replace_host);

    ui_test_utils::UrlLoadObserver guest_observer(
        guest_url, content::NotificationService::AllSources());

    LoadAndLaunchPlatformApp(app_path.c_str(), "guest-loaded");

    guest_observer.Wait();
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()->GetRenderProcessHost()->
        IsIsolatedGuest());

    content::WebContents* guest_web_contents = source->GetWebContents();
    return guest_web_contents;
  }

  // Runs media_access/allow tests.
  void MediaAccessAPIAllowTestHelper(const std::string& test_name);

  // Runs media_access/deny tests, each of them are run separately otherwise
  // they timeout (mostly on Windows).
  void MediaAccessAPIDenyTestHelper(const std::string& test_name) {
    ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
    LoadAndLaunchPlatformApp("web_view/media_access/deny", "loaded");

    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ASSERT_TRUE(embedder_web_contents);

    ExtensionTestMessageListener test_run_listener("PASSED", false);
    test_run_listener.set_failure_message("FAILED");
    EXPECT_TRUE(
        content::ExecuteScript(
            embedder_web_contents,
            base::StringPrintf("startDenyTest('%s')", test_name.c_str())));
    ASSERT_TRUE(test_run_listener.WaitUntilSatisfied());
  }

  void WaitForInterstitial(content::WebContents* web_contents) {
    scoped_refptr<content::MessageLoopRunner> loop_runner(
        new content::MessageLoopRunner);
    InterstitialObserver observer(web_contents,
                                  loop_runner->QuitClosure(),
                                  base::Closure());
    if (!content::InterstitialPage::GetInterstitialPage(web_contents))
      loop_runner->Run();
  }

  void LoadAppWithGuest(const std::string& app_path) {
    ExtensionTestMessageListener launched_listener("WebViewTest.LAUNCHED",
                                                   false);
    launched_listener.set_failure_message("WebViewTest.FAILURE");
    LoadAndLaunchPlatformApp(app_path.c_str(), &launched_listener);

    guest_web_contents_ = GetGuestViewManager()->WaitForGuestCreated();
  }

  void SendMessageToEmbedder(const std::string& message) {
    EXPECT_TRUE(
        content::ExecuteScript(
            GetEmbedderWebContents(),
            base::StringPrintf("onAppCommand('%s');", message.c_str())));
  }

  void SendMessageToGuestAndWait(const std::string& message,
                                 const std::string& wait_message) {
    scoped_ptr<ExtensionTestMessageListener> listener;
    if (!wait_message.empty()) {
      listener.reset(new ExtensionTestMessageListener(wait_message, false));
    }

    EXPECT_TRUE(
        content::ExecuteScript(
            GetGuestWebContents(),
            base::StringPrintf("onAppCommand('%s');", message.c_str())));

    if (listener) {
      ASSERT_TRUE(listener->WaitUntilSatisfied());
    }
  }

  content::WebContents* GetGuestWebContents() {
    return guest_web_contents_;
  }

  content::WebContents* GetEmbedderWebContents() {
    if (!embedder_web_contents_) {
      embedder_web_contents_ = GetFirstAppWindowWebContents();
    }
    return embedder_web_contents_;
  }

  extensions::TestGuestViewManager* GetGuestViewManager() {
    return static_cast<extensions::TestGuestViewManager*>(
        extensions::TestGuestViewManager::FromBrowserContext(
            browser()->profile()));
  }

  WebViewTest() : guest_web_contents_(NULL),
                  embedder_web_contents_(NULL) {
    extensions::GuestViewManager::set_factory_for_testing(&factory_);
  }

 private:
  bool UsesFakeSpeech() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();

    // SpeechRecognition test specific SetUp.
    return !strcmp(test_info->name(),
                   "SpeechRecognitionAPI_HasPermissionAllow");
  }

  scoped_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;

  extensions::TestGuestViewManagerFactory factory_;
  // Note that these are only set if you launch app using LoadAppWithGuest().
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
};

class WebViewDPITest : public WebViewTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", scale()));
  }

  static float scale() { return 2.0f; }
};

// This test verifies that hiding the guest triggers WebContents::WasHidden().
IN_PROC_BROWSER_TEST_F(WebViewTest, GuestVisibilityChanged) {
  LoadAppWithGuest("web_view/visibility_changed");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in platform_apps/web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-guest");
  if (!observer.hidden_observed())
    loop_runner->Run();
}

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(WebViewTest, EmbedderVisibilityChanged) {
  LoadAppWithGuest("web_view/visibility_changed");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in platform_apps/web_view/visibility_changed/main.js
  SendMessageToEmbedder("hide-embedder");
  if (!observer.hidden_observed())
    loop_runner->Run();
}

// This test verifies that reloading the embedder reloads the guest (and doest
// not crash).
IN_PROC_BROWSER_TEST_F(WebViewTest, ReloadEmbedder) {
  // Just load a guest from other test, we do not want to add a separate
  // platform_app for this test.
  LoadAppWithGuest("web_view/visibility_changed");

  ExtensionTestMessageListener launched_again_listener("WebViewTest.LAUNCHED",
                                                       false);
  GetEmbedderWebContents()->GetController().Reload(false);
  ASSERT_TRUE(launched_again_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, AcceptTouchEvents) {
  LoadAppWithGuest("web_view/accept_touch_events");

  content::RenderViewHost* embedder_rvh =
      GetEmbedderWebContents()->GetRenderViewHost();

  bool embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_FALSE(embedder_has_touch_handler);

  SendMessageToGuestAndWait("install-touch-handler", "installed-touch-handler");

  // Note that we need to wait for the installed/registered touch handler to
  // appear in browser process before querying |embedder_rvh|.
  // In practice, since we do a roundrtip from browser process to guest and
  // back, this is sufficient.
  embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_TRUE(embedder_has_touch_handler);

  SendMessageToGuestAndWait("uninstall-touch-handler",
                            "uninstalled-touch-handler");
  // Same as the note above about waiting.
  embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  EXPECT_FALSE(embedder_has_touch_handler);
}

// This test ensures JavaScript errors ("Cannot redefine property") do not
// happen when a <webview> is removed from DOM and added back.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       AddRemoveWebView_AddRemoveWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/addremove"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, AutoSize) {
#if defined(OS_WIN)
  // Flaky on XP bot http://crbug.com/299507
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/autosize"))
      << message_;
}

// Tests that a <webview> that is set to "display: none" after load and then
// setting "display: block" re-renders the plugin properly.
//
// Initially after loading the <webview> and the test sets <webview> to
// "display: none".
// This causes the browser plugin to be destroyed, we then set the
// style.display of the <webview> to block again and check that loadstop
// fires properly.
IN_PROC_BROWSER_TEST_F(WebViewTest, DisplayNoneAndBack) {
  LoadAppWithGuest("web_view/display_none_and_back");

  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  WebContentsHiddenObserver observer(GetGuestWebContents(),
                                     loop_runner->QuitClosure());

  // Handled in platform_apps/web_view/display_none_and_back/main.js
  SendMessageToEmbedder("hide-guest");
  GetGuestViewManager()->WaitForGuestDeleted();
  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED",
                                                    false);

  SendMessageToEmbedder("show-guest");
  GetGuestViewManager()->WaitForGuestCreated();
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// http://crbug.com/326332
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_Shim_TestAutosizeAfterNavigation) {
  TestHelper("testAutosizeAfterNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAllowTransparencyAttribute) {
  TestHelper("testAllowTransparencyAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewDPITest, Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

// This test is disabled due to being flaky. http://crbug.com/282116
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_Shim_TestAutosizeWithPartialAttributes \
    DISABLED_Shim_TestAutosizeWithPartialAttributes
#else
#define MAYBE_Shim_TestAutosizeWithPartialAttributes \
    Shim_TestAutosizeWithPartialAttributes
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MAYBE_Shim_TestAutosizeWithPartialAttributes) {
  TestHelper("testAutosizeWithPartialAttributes",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAPIMethodExistence) {
  TestHelper("testAPIMethodExistence", "web_view/shim", NO_TEST_SERVER);
}

// Tests the existence of WebRequest API event objects on the request
// object, on the webview element, and hanging directly off webview.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIExistence) {
  TestHelper("testWebRequestAPIExistence", "web_view/shim", NO_TEST_SERVER);
}

// http://crbug.com/315920
#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Shim_TestChromeExtensionURL DISABLED_Shim_TestChromeExtensionURL
#else
#define MAYBE_Shim_TestChromeExtensionURL Shim_TestChromeExtensionURL
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_Shim_TestChromeExtensionURL) {
  TestHelper("testChromeExtensionURL", "web_view/shim", NO_TEST_SERVER);
}

// http://crbug.com/315920
#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Shim_TestChromeExtensionRelativePath \
    DISABLED_Shim_TestChromeExtensionRelativePath
#else
#define MAYBE_Shim_TestChromeExtensionRelativePath \
    Shim_TestChromeExtensionRelativePath
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MAYBE_Shim_TestChromeExtensionRelativePath) {
  TestHelper("testChromeExtensionRelativePath",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDisplayNoneWebviewLoad) {
  TestHelper("testDisplayNoneWebviewLoad", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDisplayNoneWebviewRemoveChild) {
  TestHelper("testDisplayNoneWebviewRemoveChild",
             "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestInlineScriptFromAccessibleResources) {
  TestHelper("testInlineScriptFromAccessibleResources",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestInvalidChromeExtensionURL) {
  TestHelper("testInvalidChromeExtensionURL", "web_view/shim", NO_TEST_SERVER);
}

// Disable on Chrome OS, as it is flaking a lot. See: http://crbug.com/413618.
#if defined(OS_CHROMEOS)
#define MAYBE_Shim_TestEventName DISABLED_Shim_TestEventName
#else
#define MAYBE_Shim_TestEventName Shim_TestEventName
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_Shim_TestEventName) {
  TestHelper("testEventName", "web_view/shim", NO_TEST_SERVER);
}

// WebViewTest.Shim_TestOnEventProperty is flaky, so disable it.
// http://crbug.com/359832.
IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_Shim_TestOnEventProperty) {
  TestHelper("testOnEventProperties", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadProgressEvent) {
  TestHelper("testLoadProgressEvent", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDestroyOnEventListener) {
  TestHelper("testDestroyOnEventListener", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestCannotMutateEventName) {
  TestHelper("testCannotMutateEventName", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestPartitionRaisesException) {
  TestHelper("testPartitionRaisesException", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestPartitionRemovalAfterNavigationFails) {
  TestHelper("testPartitionRemovalAfterNavigationFails",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestExecuteScriptFail) {
#if defined(OS_WIN)
  // Flaky on XP bot http://crbug.com/266185
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  TestHelper("testExecuteScriptFail", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestExecuteScript) {
  TestHelper("testExecuteScript", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    Shim_TestExecuteScriptIsAbortedWhenWebViewSourceIsChanged) {
  TestHelper("testExecuteScriptIsAbortedWhenWebViewSourceIsChanged",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestTerminateAfterExit) {
  TestHelper("testTerminateAfterExit", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestAssignSrcAfterCrash) {
  TestHelper("testAssignSrcAfterCrash", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestNavOnConsecutiveSrcAttributeChanges) {
  TestHelper("testNavOnConsecutiveSrcAttributeChanges",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNavOnSrcAttributeChange) {
  TestHelper("testNavOnSrcAttributeChange", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNavigateAfterResize) {
  TestHelper("testNavigateAfterResize", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveSrcAttribute) {
  TestHelper("testRemoveSrcAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestReassignSrcAttribute) {
  TestHelper("testReassignSrcAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNewWindow) {
  TestHelper("testNewWindow", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNewWindowTwoListeners) {
  TestHelper("testNewWindowTwoListeners", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNewWindowNoPreventDefault) {
  TestHelper("testNewWindowNoPreventDefault",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNewWindowNoReferrerLink) {
  TestHelper("testNewWindowNoReferrerLink", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestContentLoadEvent) {
  TestHelper("testContentLoadEvent", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestDeclarativeWebRequestAPI) {
  TestHelper("testDeclarativeWebRequestAPI",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestDeclarativeWebRequestAPISendMessage) {
  TestHelper("testDeclarativeWebRequestAPISendMessage",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPI) {
  TestHelper("testWebRequestAPI", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestWebRequestAPIGoogleProperty) {
  TestHelper("testWebRequestAPIGoogleProperty",
             "web_view/shim",
             NO_TEST_SERVER);
}

// This test is disabled due to being flaky. http://crbug.com/309451
#if defined(OS_WIN)
#define MAYBE_Shim_TestWebRequestListenerSurvivesReparenting \
    DISABLED_Shim_TestWebRequestListenerSurvivesReparenting
#else
#define MAYBE_Shim_TestWebRequestListenerSurvivesReparenting \
    Shim_TestWebRequestListenerSurvivesReparenting
#endif
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    MAYBE_Shim_TestWebRequestListenerSurvivesReparenting) {
  TestHelper("testWebRequestListenerSurvivesReparenting",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadStartLoadRedirect) {
  TestHelper("testLoadStartLoadRedirect", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       Shim_TestLoadAbortChromeExtensionURLWrongPartition) {
  TestHelper("testLoadAbortChromeExtensionURLWrongPartition",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortEmptyResponse) {
  TestHelper("testLoadAbortEmptyResponse", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortIllegalChromeURL) {
  TestHelper("testLoadAbortIllegalChromeURL",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortIllegalFileURL) {
  TestHelper("testLoadAbortIllegalFileURL", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortIllegalJavaScriptURL) {
  TestHelper("testLoadAbortIllegalJavaScriptURL",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortInvalidNavigation) {
  TestHelper("testLoadAbortInvalidNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestLoadAbortNonWebSafeScheme) {
  TestHelper("testLoadAbortNonWebSafeScheme", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestReload) {
  TestHelper("testReload", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestReloadAfterTerminate) {
  TestHelper("testReloadAfterTerminate", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestGetProcessId) {
  TestHelper("testGetProcessId", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestHiddenBeforeNavigation) {
  TestHelper("testHiddenBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveWebviewOnExit) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  // Launch the app and wait until it's ready to load a test.
  LoadAndLaunchPlatformApp("web_view/shim", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);

  std::string guest_path(
      "/extensions/platform_apps/web_view/shim/empty_guest.html");
  GURL guest_url = embedded_test_server()->GetURL(guest_path);
  guest_url = guest_url.ReplaceComponents(replace_host);

  ui_test_utils::UrlLoadObserver guest_observer(
      guest_url, content::NotificationService::AllSources());

  // Run the test and wait until the guest WebContents is available and has
  // finished loading.
  ExtensionTestMessageListener guest_loaded_listener("guest-loaded", false);
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "runTest('testRemoveWebviewOnExit')"));
  guest_observer.Wait();

  content::Source<content::NavigationController> source =
      guest_observer.source();
  EXPECT_TRUE(source->GetWebContents()->GetRenderProcessHost()->
      IsIsolatedGuest());

  ASSERT_TRUE(guest_loaded_listener.WaitUntilSatisfied());

  content::WebContentsDestroyedWatcher destroyed_watcher(
      source->GetWebContents());

  // Tell the embedder to kill the guest.
  EXPECT_TRUE(content::ExecuteScript(
                  embedder_web_contents,
                  "removeWebviewOnExitDoCrash();"));

  // Wait until the guest WebContents is destroyed.
  destroyed_watcher.Wait();
}

// Remove <webview> immediately after navigating it.
// This is a regression test for http://crbug.com/276023.
IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestRemoveWebviewAfterNavigation) {
  TestHelper("testRemoveWebviewAfterNavigation",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestNavigationToExternalProtocol) {
  TestHelper("testNavigationToExternalProtocol",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestResizeWebviewResizesContent) {
  TestHelper("testResizeWebviewResizesContent",
             "web_view/shim",
             NO_TEST_SERVER);
}

// This test makes sure we do not crash if app is closed while interstitial
// page is being shown in guest.
// Disabled under LeakSanitizer due to memory leaks. http://crbug.com/321662
#if defined(LEAK_SANITIZER)
#define MAYBE_InterstitialTeardown DISABLED_InterstitialTeardown
#else
#define MAYBE_InterstitialTeardown InterstitialTeardown
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_InterstitialTeardown) {
#if defined(OS_WIN)
  // Flaky on XP bot http://crbug.com/297014
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  // Start a HTTPS server so we can load an interstitial page inside guest.
  net::SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.server_certificate =
      net::SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME;
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS, ssl_options,
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());

  net::HostPortPair host_and_port = https_server.host_port_pair();

  LoadAndLaunchPlatformApp("web_view/interstitial_teardown", "EmbedderLoaded");

  // Now load the guest.
  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ExtensionTestMessageListener second("GuestAddedToDom", false);
  EXPECT_TRUE(content::ExecuteScript(
      embedder_web_contents,
      base::StringPrintf("loadGuest(%d);\n", host_and_port.port())));
  ASSERT_TRUE(second.WaitUntilSatisfied());

  // Wait for interstitial page to be shown in guest.
  content::WebContents* guest_web_contents =
      GetGuestViewManager()->WaitForGuestCreated();
  ASSERT_TRUE(guest_web_contents->GetRenderProcessHost()->IsIsolatedGuest());
  WaitForInterstitial(guest_web_contents);

  // Now close the app while interstitial page being shown in guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ShimSrcAttribute) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/src_attribute"))
      << message_;
}

// This test verifies that prerendering has been disabled inside <webview>.
// This test is here rather than in PrerenderBrowserTest for testing convenience
// only. If it breaks then this is a bug in the prerenderer.
IN_PROC_BROWSER_TEST_F(WebViewTest, NoPrerenderer) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebContents* guest_web_contents =
      LoadGuest(
          "/extensions/platform_apps/web_view/noprerenderer/guest.html",
          "web_view/noprerenderer");
  ASSERT_TRUE(guest_web_contents != NULL);

  PrerenderLinkManager* prerender_link_manager =
      PrerenderLinkManagerFactory::GetForProfile(
          Profile::FromBrowserContext(guest_web_contents->GetBrowserContext()));
  ASSERT_TRUE(prerender_link_manager != NULL);
  EXPECT_TRUE(prerender_link_manager->IsEmpty());
}

// Verify that existing <webview>'s are detected when the task manager starts
// up.
IN_PROC_BROWSER_TEST_F(WebViewTest, TaskManagerExistingWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadGuest("/extensions/platform_apps/web_view/task_manager/guest.html",
            "web_view/task_manager");

  chrome::ShowTaskManager(browser());  // Show task manager AFTER guest loads.

  const char* guest_title = "WebViewed test content";
  const char* app_name = "<webview> task manager test";
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchWebView(guest_title)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp(app_name)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchBackground(app_name)));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyWebView()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyBackground()));
}

// Verify that the task manager notices the creation of new <webview>'s.
IN_PROC_BROWSER_TEST_F(WebViewTest, TaskManagerNewWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  chrome::ShowTaskManager(browser());  // Show task manager BEFORE guest loads.

  LoadGuest("/extensions/platform_apps/web_view/task_manager/guest.html",
            "web_view/task_manager");

  const char* guest_title = "WebViewed test content";
  const char* app_name = "<webview> task manager test";
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchWebView(guest_title)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp(app_name)));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchBackground(app_name)));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyWebView()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyBackground()));
}

// This tests cookie isolation for packaged apps with webview tags. It navigates
// the main browser window to a page that sets a cookie and loads an app with
// multiple webview tags. Each tag sets a cookie and the test checks the proper
// storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, CookieIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const std::string kExpire =
      "var expire = new Date(Date.now() + 24 * 60 * 60 * 1000);";
  std::string cookie_script1(kExpire);
  cookie_script1.append(
      "document.cookie = 'guest1=true; path=/; expires=' + expire + ';';");
  std::string cookie_script2(kExpire);
  cookie_script2.append(
      "document.cookie = 'guest2=true; path=/; expires=' + expire + ';';");

  GURL::Replacements replace_host;
  std::string host_str("localhost");  // Must stay in scope with replace_host.
  replace_host.SetHostStr(host_str);

  GURL set_cookie_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/isolation/set_cookie.html");
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  // The first two partitions will be used to set cookies and ensure they are
  // shared. The named partition is used to ensure that cookies are isolated
  // between partitions within the same app.
  content::WebContents* cookie_contents1;
  content::WebContents* cookie_contents2;
  content::WebContents* named_partition_contents1;
  content::WebContents* named_partition_contents2;

  NavigateAndOpenAppForIsolation(set_cookie_url, &cookie_contents1,
                                 &cookie_contents2, &named_partition_contents1,
                                 &named_partition_contents2, NULL, NULL, NULL);

  EXPECT_TRUE(content::ExecuteScript(cookie_contents1, cookie_script1));
  EXPECT_TRUE(content::ExecuteScript(cookie_contents2, cookie_script2));

  int cookie_size;
  std::string cookie_value;

  // Test the regular browser context to ensure we have only one cookie.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            browser()->tab_strip_model()->GetWebContentsAt(0),
                            &cookie_size, &cookie_value);
  EXPECT_EQ("testCookie=1", cookie_value);

  // The default behavior is to combine webview tags with no explicit partition
  // declaration into the same in-memory partition. Test the webview tags to
  // ensure we have properly set the cookies and we have both cookies in both
  // tags.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            cookie_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("guest1=true; guest2=true", cookie_value);

  ui_test_utils::GetCookies(GURL("http://localhost"),
                            cookie_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("guest1=true; guest2=true", cookie_value);

  // The third tag should not have any cookies as it is in a separate partition.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            named_partition_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
}

// This tests that in-memory storage partitions are reset on browser restart,
// but persistent ones maintain state for cookies and HTML5 storage.
IN_PROC_BROWSER_TEST_F(WebViewTest, PRE_StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const std::string kExpire =
      "var expire = new Date(Date.now() + 24 * 60 * 60 * 1000);";
  std::string cookie_script1(kExpire);
  cookie_script1.append(
      "document.cookie = 'inmemory=true; path=/; expires=' + expire + ';';");
  std::string cookie_script2(kExpire);
  cookie_script2.append(
      "document.cookie = 'persist1=true; path=/; expires=' + expire + ';';");
  std::string cookie_script3(kExpire);
  cookie_script3.append(
      "document.cookie = 'persist2=true; path=/; expires=' + expire + ';';");

  // We don't care where the main browser is on this test.
  GURL blank_url("about:blank");

  // The first two partitions will be used to set cookies and ensure they are
  // shared. The named partition is used to ensure that cookies are isolated
  // between partitions within the same app.
  content::WebContents* cookie_contents1;
  content::WebContents* cookie_contents2;
  content::WebContents* named_partition_contents1;
  content::WebContents* named_partition_contents2;
  content::WebContents* persistent_partition_contents1;
  content::WebContents* persistent_partition_contents2;
  content::WebContents* persistent_partition_contents3;
  NavigateAndOpenAppForIsolation(blank_url, &cookie_contents1,
                                 &cookie_contents2, &named_partition_contents1,
                                 &named_partition_contents2,
                                 &persistent_partition_contents1,
                                 &persistent_partition_contents2,
                                 &persistent_partition_contents3);

  // Set the inmemory=true cookie for tags with inmemory partitions.
  EXPECT_TRUE(content::ExecuteScript(cookie_contents1, cookie_script1));
  EXPECT_TRUE(content::ExecuteScript(named_partition_contents1,
                                     cookie_script1));

  // For the two different persistent storage partitions, set the
  // two different cookies so we can check that they aren't comingled below.
  EXPECT_TRUE(content::ExecuteScript(persistent_partition_contents1,
                                     cookie_script2));

  EXPECT_TRUE(content::ExecuteScript(persistent_partition_contents3,
                                     cookie_script3));

  int cookie_size;
  std::string cookie_value;

  // Check that all in-memory partitions have a cookie set.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            cookie_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            cookie_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            named_partition_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            named_partition_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("inmemory=true", cookie_value);

  // Check that all persistent partitions kept their state.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            persistent_partition_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            persistent_partition_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            persistent_partition_contents3,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("persist2=true", cookie_value);
}

// This is the post-reset portion of the StoragePersistence test.  See
// PRE_StoragePersistence for main comment.
#if defined(OS_CHROMEOS)
// http://crbug.com/223888
#define MAYBE_StoragePersistence DISABLED_StoragePersistence
#else
#define MAYBE_StoragePersistence StoragePersistence
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // We don't care where the main browser is on this test.
  GURL blank_url("about:blank");

  // The first two partitions will be used to set cookies and ensure they are
  // shared. The named partition is used to ensure that cookies are isolated
  // between partitions within the same app.
  content::WebContents* cookie_contents1;
  content::WebContents* cookie_contents2;
  content::WebContents* named_partition_contents1;
  content::WebContents* named_partition_contents2;
  content::WebContents* persistent_partition_contents1;
  content::WebContents* persistent_partition_contents2;
  content::WebContents* persistent_partition_contents3;
  NavigateAndOpenAppForIsolation(blank_url, &cookie_contents1,
                                 &cookie_contents2, &named_partition_contents1,
                                 &named_partition_contents2,
                                 &persistent_partition_contents1,
                                 &persistent_partition_contents2,
                                 &persistent_partition_contents3);

  int cookie_size;
  std::string cookie_value;

  // Check that all in-memory partitions lost their state.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            cookie_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            cookie_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            named_partition_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            named_partition_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("", cookie_value);

  // Check that all persistent partitions kept their state.
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            persistent_partition_contents1,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            persistent_partition_contents2,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("persist1=true", cookie_value);
  ui_test_utils::GetCookies(GURL("http://localhost"),
                            persistent_partition_contents3,
                            &cookie_size, &cookie_value);
  EXPECT_EQ("persist2=true", cookie_value);
}

// This tests DOM storage isolation for packaged apps with webview tags. It
// loads an app with multiple webview tags and each tag sets DOM storage
// entries, which the test checks to ensure proper storage isolation is
// enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, DOMStorageIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL regular_url = embedded_test_server()->GetURL("/title1.html");

  std::string output;
  std::string get_local_storage("window.domAutomationController.send("
      "window.localStorage.getItem('foo') || 'badval')");
  std::string get_session_storage("window.domAutomationController.send("
      "window.sessionStorage.getItem('bar') || 'badval')");

  content::WebContents* default_tag_contents1;
  content::WebContents* default_tag_contents2;
  content::WebContents* storage_contents1;
  content::WebContents* storage_contents2;

  NavigateAndOpenAppForIsolation(regular_url, &default_tag_contents1,
                                 &default_tag_contents2, &storage_contents1,
                                 &storage_contents2, NULL, NULL, NULL);

  // Initialize the storage for the first of the two tags that share a storage
  // partition.
  EXPECT_TRUE(content::ExecuteScript(storage_contents1,
                                     "initDomStorage('page1')"));

  // Let's test that the expected values are present in the first tag, as they
  // will be overwritten once we call the initDomStorage on the second tag.
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("local-page1", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("session-page1", output.c_str());

  // Now, init the storage in the second tag in the same storage partition,
  // which will overwrite the shared localStorage.
  EXPECT_TRUE(content::ExecuteScript(storage_contents2,
                                     "initDomStorage('page2')"));

  // The localStorage value now should reflect the one written through the
  // second tag.
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("local-page2", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("local-page2", output.c_str());

  // Session storage is not shared though, as each webview tag has separate
  // instance, even if they are in the same storage partition.
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("session-page1", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("session-page2", output.c_str());

  // Also, let's check that the main browser and another tag that doesn't share
  // the same partition don't have those values stored.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_local_storage.c_str(),
      &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_session_storage.c_str(),
      &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(default_tag_contents1,
                                            get_local_storage.c_str(),
                                            &output));
  EXPECT_STREQ("badval", output.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractString(default_tag_contents1,
                                            get_session_storage.c_str(),
                                            &output));
  EXPECT_STREQ("badval", output.c_str());
}

// This tests IndexedDB isolation for packaged apps with webview tags. It loads
// an app with multiple webview tags and each tag creates an IndexedDB record,
// which the test checks to ensure proper storage isolation is enforced.
IN_PROC_BROWSER_TEST_F(WebViewTest, IndexedDBIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL regular_url = embedded_test_server()->GetURL("/title1.html");

  content::WebContents* default_tag_contents1;
  content::WebContents* default_tag_contents2;
  content::WebContents* storage_contents1;
  content::WebContents* storage_contents2;

  NavigateAndOpenAppForIsolation(regular_url, &default_tag_contents1,
                                 &default_tag_contents2, &storage_contents1,
                                 &storage_contents2, NULL, NULL, NULL);

  // Initialize the storage for the first of the two tags that share a storage
  // partition.
  ExecuteScriptWaitForTitle(storage_contents1, "initIDB()", "idb created");
  ExecuteScriptWaitForTitle(storage_contents1, "addItemIDB(7, 'page1')",
                            "addItemIDB complete");
  ExecuteScriptWaitForTitle(storage_contents1, "readItemIDB(7)",
                            "readItemIDB complete");

  std::string output;
  std::string get_value(
      "window.domAutomationController.send(getValueIDB() || 'badval')");

  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page1", output.c_str());

  // Initialize the db in the second tag.
  ExecuteScriptWaitForTitle(storage_contents2, "initIDB()", "idb open");

  // Since we share a partition, reading the value should return the existing
  // one.
  ExecuteScriptWaitForTitle(storage_contents2, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page1", output.c_str());

  // Now write through the second tag and read it back.
  ExecuteScriptWaitForTitle(storage_contents2, "addItemIDB(7, 'page2')",
                            "addItemIDB complete");
  ExecuteScriptWaitForTitle(storage_contents2, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents2,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page2", output.c_str());

  // Reset the document title, otherwise the next call will not see a change and
  // will hang waiting for it.
  EXPECT_TRUE(content::ExecuteScript(storage_contents1,
                                     "document.title = 'foo'"));

  // Read through the first tag to ensure we have the second value.
  ExecuteScriptWaitForTitle(storage_contents1, "readItemIDB(7)",
                            "readItemIDB complete");
  EXPECT_TRUE(ExecuteScriptAndExtractString(storage_contents1,
                                            get_value.c_str(), &output));
  EXPECT_STREQ("page2", output.c_str());

  // Now, let's confirm there is no database in the main browser and another
  // tag that doesn't share the same partition. Due to the IndexedDB API design,
  // open will succeed, but the version will be 1, since it creates the database
  // if it is not found. The two tags use database version 3, so we avoid
  // ambiguity.
  const char* script =
      "indexedDB.open('isolation').onsuccess = function(e) {"
      "  if (e.target.result.version == 1)"
      "    document.title = 'db not found';"
      "  else "
      "    document.title = 'error';"
      "}";
  ExecuteScriptWaitForTitle(browser()->tab_strip_model()->GetWebContentsAt(0),
                            script, "db not found");
  ExecuteScriptWaitForTitle(default_tag_contents1, script, "db not found");
}

// This test ensures that closing app window on 'loadcommit' does not crash.
// The test launches an app with guest and closes the window on loadcommit. It
// then launches the app window again. The process is repeated 3 times.
// http://crbug.com/291278
#if defined(OS_WIN)
#define MAYBE_CloseOnLoadcommit DISABLED_CloseOnLoadcommit
#else
#define MAYBE_CloseOnLoadcommit CloseOnLoadcommit
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_CloseOnLoadcommit) {
  LoadAndLaunchPlatformApp("web_view/close_on_loadcommit",
                           "done-close-on-loadcommit");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIDeny_TestDeny) {
  MediaAccessAPIDenyTestHelper("testDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestDenyThenAllowThrows) {
  MediaAccessAPIDenyTestHelper("testDenyThenAllowThrows");

}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestDenyWithPreventDefault) {
  MediaAccessAPIDenyTestHelper("testDenyWithPreventDefault");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestNoListenersImplyDeny) {
  MediaAccessAPIDenyTestHelper("testNoListenersImplyDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewTest,
                       MediaAccessAPIDeny_TestNoPreventDefaultImpliesDeny) {
  MediaAccessAPIDenyTestHelper("testNoPreventDefaultImpliesDeny");
}

void WebViewTest::MediaAccessAPIAllowTestHelper(const std::string& test_name) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/media_access/allow", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  scoped_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents->SetDelegate(mock.get());

  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(
      content::ExecuteScript(
          embedder_web_contents,
          base::StringPrintf("startAllowTest('%s')",
                             test_name.c_str())));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForSetMediaPermission();
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ContextMenusAPI_Basic) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // 1. Basic property test.
  ExecuteScriptWaitForTitle(embedder, "checkProperties()", "ITEM_CHECKED");

  // 2. Create a menu item and wait for created callback to be called.
  ExecuteScriptWaitForTitle(embedder, "createMenuItem()", "ITEM_CREATED");

  // 3. Click the created item, wait for the click handlers to fire from JS.
  ExtensionTestMessageListener click_listener("ITEM_CLICKED", false);
  GURL page_url("http://www.google.com");
  // Create and build our test context menu.
  scoped_ptr<TestRenderViewContextMenu> menu(TestRenderViewContextMenu::Create(
      guest_web_contents, page_url, GURL(), GURL()));

  // Look for the extension item in the menu, and execute it.
  int command_id = ContextMenuMatcher::ConvertToExtensionsCustomCommandId(0);
  ASSERT_TRUE(menu->IsCommandIdEnabled(command_id));
  menu->ExecuteCommand(command_id, 0);

  // Wait for embedder's script to tell us its onclick fired, it does
  // chrome.test.sendMessage('ITEM_CLICKED')
  ASSERT_TRUE(click_listener.WaitUntilSatisfied());

  // 4. Update the item's title and verify.
  ExecuteScriptWaitForTitle(embedder, "updateMenuItem()", "ITEM_UPDATED");
  MenuItem::List items = GetItems();
  ASSERT_EQ(1u, items.size());
  MenuItem* item = items.at(0);
  EXPECT_EQ("new_title", item->title());

  // 5. Remove the item.
  ExecuteScriptWaitForTitle(embedder, "removeItem()", "ITEM_REMOVED");
  MenuItem::List items_after_removal = GetItems();
  ASSERT_EQ(0u, items_after_removal.size());

  // 6. Add some more items.
  ExecuteScriptWaitForTitle(
      embedder, "createThreeMenuItems()", "ITEM_MULTIPLE_CREATED");
  MenuItem::List items_after_insertion = GetItems();
  ASSERT_EQ(3u, items_after_insertion.size());

  // 7. Test removeAll().
  ExecuteScriptWaitForTitle(embedder, "removeAllItems()", "ITEM_ALL_REMOVED");
  MenuItem::List items_after_all_removal = GetItems();
  ASSERT_EQ(0u, items_after_all_removal.size());
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllow) {
  MediaAccessAPIAllowTestHelper("testAllow");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllowAndThenDeny) {
  MediaAccessAPIAllowTestHelper("testAllowAndThenDeny");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllowTwice) {
  MediaAccessAPIAllowTestHelper("testAllowTwice");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, MediaAccessAPIAllow_TestAllowAsync) {
  MediaAccessAPIAllowTestHelper("testAllowAsync");
}

// Checks that window.screenX/screenY/screenLeft/screenTop works correctly for
// guests.
IN_PROC_BROWSER_TEST_F(WebViewTest, ScreenCoordinates) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "screen_coordinates"))
          << message_;
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(WebViewTest, ChromeVoxInjection) {
  EXPECT_FALSE(
      chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebContents* guest_web_contents = LoadGuest(
      "/extensions/platform_apps/web_view/chromevox_injection/guest.html",
      "web_view/chromevox_injection");
  ASSERT_TRUE(guest_web_contents);

  chromeos::SpeechMonitor monitor;
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ash::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(monitor.SkipChromeVoxEnabledMessage());

  EXPECT_EQ("chrome vox test title", monitor.GetNextUtterance());
}
#endif

// Flaky on Windows. http://crbug.com/303966
#if defined(OS_WIN)
#define MAYBE_TearDownTest DISABLED_TearDownTest
#else
#define MAYBE_TearDownTest TearDownTest
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_TearDownTest) {
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("web_view/teardown", "guest-loaded");
  extensions::AppWindow* window = NULL;
  if (!GetAppWindowCount())
    window = CreateAppWindow(extension);
  else
    window = GetFirstAppWindow();
  CloseAppWindow(window);

  // Load the app again.
  LoadAndLaunchPlatformApp("web_view/teardown", "guest-loaded");
}

// In following GeolocationAPIEmbedderHasNoAccess* tests, embedder (i.e. the
// platform app) does not have geolocation permission for this test.
// No matter what the API does, geolocation permission would be denied.
// Note that the test name prefix must be "GeolocationAPI".
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasNoAccessAllow) {
  TestHelper("testDenyDenies",
             "web_view/geolocation/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasNoAccessDeny) {
  TestHelper("testDenyDenies",
             "web_view/geolocation/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

// In following GeolocationAPIEmbedderHasAccess* tests, embedder (i.e. the
// platform app) has geolocation permission
//
// Note that these test names must be "GeolocationAPI" prefixed (b/c we mock out
// geolocation in this case).
//
// Also note that these are run separately because OverrideGeolocation() doesn't
// mock out geolocation for multiple navigator.geolocation calls properly and
// the tests become flaky.
// GeolocationAPI* test 1 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasAccessAllow) {
  TestHelper("testAllow",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// GeolocationAPI* test 2 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPIEmbedderHasAccessDeny) {
  TestHelper("testDeny",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// GeolocationAPI* test 3 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       GeolocationAPIEmbedderHasAccessMultipleBridgeIdAllow) {
  TestHelper("testMultipleBridgeIdAllow",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// Tests that
// BrowserPluginGeolocationPermissionContext::CancelGeolocationPermissionRequest
// is handled correctly (and does not crash).
IN_PROC_BROWSER_TEST_F(WebViewTest, GeolocationAPICancelGeolocation) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest(
        "platform_apps/web_view/geolocation/cancel_request")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DISABLED_GeolocationRequestGone) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest(
        "platform_apps/web_view/geolocation/geolocation_request_gone"))
            << message_;
}

// In following FilesystemAPIRequestFromMainThread* tests, guest request
// filesystem access from main thread of the guest.
// FileSystemAPIRequestFromMainThread* test 1 of 3
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromMainThreadAllow) {
  TestHelper("testAllow", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromMainThread* test 2 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromMainThreadDeny) {
  TestHelper("testDeny", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromMainThread* test 3 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       FileSystemAPIRequestFromMainThreadDefaultAllow) {
  TestHelper("testDefaultAllow", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromWorker* tests, guest create a worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromWorker* test 1 of 3
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromWorkerAllow) {
  TestHelper("testAllow", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromWorker* test 2 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest, FileSystemAPIRequestFromWorkerDeny) {
  TestHelper("testDeny", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromWorker* test 3 of 3.
IN_PROC_BROWSER_TEST_F(WebViewTest,
                       FileSystemAPIRequestFromWorkerDefaultAllow) {
  TestHelper(
      "testDefaultAllow", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* tests,
// embedder contains a single webview guest. The guest creates a shared worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 1 of 3
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestAllow) {
  TestHelper("testAllow",
             "web_view/filesystem/shared_worker/single",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 2 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestDeny) {
  TestHelper("testDeny",
             "web_view/filesystem/shared_worker/single",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 3 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestDefaultAllow) {
  TestHelper(
      "testDefaultAllow",
      "web_view/filesystem/shared_worker/single",
      NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* tests,
// embedder contains mutiple webview guests. Each guest creates a shared worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 1 of 3
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsAllow) {
  TestHelper("testAllow",
             "web_view/filesystem/shared_worker/multiple",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 2 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsDeny) {
  TestHelper("testDeny",
             "web_view/filesystem/shared_worker/multiple",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 3 of 3.
IN_PROC_BROWSER_TEST_F(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsDefaultAllow) {
  TestHelper(
      "testDefaultAllow",
      "web_view/filesystem/shared_worker/multiple",
      NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, ClearData) {
#if defined(OS_WIN)
  // Flaky on XP bot http://crbug.com/282674
  if (base::win::GetVersion() <= base::win::VERSION_XP)
    return;
#endif

  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "cleardata"))
          << message_;
}

// This test is disabled on Win due to being flaky. http://crbug.com/294592
#if defined(OS_WIN)
#define MAYBE_ConsoleMessage DISABLED_ConsoleMessage
#else
#define MAYBE_ConsoleMessage ConsoleMessage
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_ConsoleMessage) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "console_messages"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, DownloadPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  content::WebContents* guest_web_contents =
      LoadGuest("/extensions/platform_apps/web_view/download/guest.html",
                "web_view/download");
  ASSERT_TRUE(guest_web_contents);

  // Replace WebContentsDelegate with mock version so we can intercept download
  // requests.
  content::WebContentsDelegate* delegate = guest_web_contents->GetDelegate();
  scoped_ptr<MockDownloadWebContentsDelegate>
      mock_delegate(new MockDownloadWebContentsDelegate(delegate));
  guest_web_contents->SetDelegate(mock_delegate.get());

  // Start test.
  // 1. Guest requests a download that its embedder denies.
  EXPECT_TRUE(content::ExecuteScript(guest_web_contents,
                                     "startDownload('download-link-1')"));
  mock_delegate->WaitForCanDownload(false); // Expect to not allow.
  mock_delegate->Reset();

  // 2. Guest requests a download that its embedder allows.
  EXPECT_TRUE(content::ExecuteScript(guest_web_contents,
                                     "startDownload('download-link-2')"));
  mock_delegate->WaitForCanDownload(true); // Expect to allow.
  mock_delegate->Reset();

  // 3. Guest requests a download that its embedder ignores, this implies deny.
  EXPECT_TRUE(content::ExecuteScript(guest_web_contents,
                                     "startDownload('download-link-3')"));
  mock_delegate->WaitForCanDownload(false); // Expect to not allow.
}

// This test makes sure loading <webview> does not crash when there is an
// extension which has content script whitelisted/forced.
IN_PROC_BROWSER_TEST_F(WebViewTest, WhitelistedContentScript) {
  // Whitelist the extension for running content script we are going to load.
  extensions::ExtensionsClient::ScriptingWhitelist whitelist;
  const std::string extension_id = "imeongpbjoodlnmlakaldhlcmijmhpbb";
  whitelist.push_back(extension_id);
  extensions::ExtensionsClient::Get()->SetScriptingWhitelist(whitelist);

  // Load the extension.
  const extensions::Extension* content_script_whitelisted_extension =
      LoadExtension(test_data_dir_.AppendASCII(
                        "platform_apps/web_view/extension_api/content_script"));
  ASSERT_TRUE(content_script_whitelisted_extension);
  ASSERT_EQ(extension_id, content_script_whitelisted_extension->id());

  // Now load an app with <webview>.
  LoadAndLaunchPlatformApp("web_view/content_script_whitelisted",
                           "TEST_PASSED");
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetPropertyOnDocumentReady) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/document_ready"))
                  << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SetPropertyOnDocumentInteractive) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/document_interactive"))
                  << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SpeechRecognitionAPI_HasPermissionAllow) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/speech_recognition_api",
                                "allowTest"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SpeechRecognitionAPI_HasPermissionDeny) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/speech_recognition_api",
                                "denyTest"))
          << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, SpeechRecognitionAPI_NoPermission) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/common",
                                "speech_recognition_api_no_permission"))
          << message_;
}

// Tests overriding user agent.
IN_PROC_BROWSER_TEST_F(WebViewTest, UserAgent) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
              "platform_apps/web_view/common", "useragent")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, UserAgent_NewWindow) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
              "platform_apps/web_view/common",
              "useragent_newwindow")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, NoPermission) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/nopermission"))
                  << message_;
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestAlertDialog) {
  TestHelper("testAlertDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, TestConfirmDialog) {
  TestHelper("testConfirmDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestConfirmDialogCancel) {
  TestHelper("testConfirmDialogCancel", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestConfirmDialogDefaultCancel) {
  TestHelper("testConfirmDialogDefaultCancel",
             "web_view/dialog",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestConfirmDialogDefaultGCCancel) {
  TestHelper("testConfirmDialogDefaultGCCancel",
             "web_view/dialog",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Dialog_TestPromptDialog) {
  TestHelper("testPromptDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, NoContentSettingsAPI) {
  // Load the extension.
  const extensions::Extension* content_settings_extension =
      LoadExtension(
          test_data_dir_.AppendASCII(
              "platform_apps/web_view/extension_api/content_settings"));
  ASSERT_TRUE(content_settings_extension);
  TestHelper("testPostMessageCommChannel", "web_view/shim", NO_TEST_SERVER);
}

#if defined(ENABLE_PLUGINS)
class WebViewPluginTest : public WebViewTest {
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebViewTest::SetUpCommandLine(command_line);

    // Append the switch to register the pepper plugin.
    // library name = <out dir>/<test_name>.<library_extension>
    // MIME type = application/x-ppapi-<test_name>
    base::FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));

    base::FilePath plugin_lib = plugin_dir.Append(library_name);
    EXPECT_TRUE(base::PathExists(plugin_lib));
    base::FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL(";application/x-ppapi-tests"));
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }
};

IN_PROC_BROWSER_TEST_F(WebViewPluginTest, TestLoadPluginEvent) {
  TestHelper("testPluginLoadPermission", "web_view/shim", NO_TEST_SERVER);
}
#endif  // defined(ENABLE_PLUGINS)

class WebViewCaptureTest : public WebViewTest {
 public:
  WebViewCaptureTest() {}
  virtual ~WebViewCaptureTest() {}
  virtual void SetUp() OVERRIDE {
    EnablePixelOutput();
    WebViewTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestZoomAPI) {
  TestHelper("testZoomAPI", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestFindAPI) {
  TestHelper("testFindAPI", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_F(WebViewTest, Shim_TestFindAPI_findupdate) {
  TestHelper("testFindAPI_findupdate", "web_view/shim", NO_TEST_SERVER);
}

// <webview> screenshot capture fails with ubercomp.
// See http://crbug.com/327035.
IN_PROC_BROWSER_TEST_F(WebViewCaptureTest,
                       DISABLED_Shim_ScreenshotCapture) {
  TestHelper("testScreenshotCapture", "web_view/shim", NO_TEST_SERVER);
}

#if defined(OS_WIN)
// Test is disabled on Windows because it times out often.
// http://crbug.com/403325
#define MAYBE_WebViewInBackgroundPage \
    DISABLED_WebViewInBackgroundPage
#else
#define MAYBE_WebViewInBackgroundPage WebViewInBackgroundPage
#endif
IN_PROC_BROWSER_TEST_F(WebViewTest, MAYBE_WebViewInBackgroundPage) {
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/background"))
      << message_;
}
