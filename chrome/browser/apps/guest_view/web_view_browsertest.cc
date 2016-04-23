// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>
#include <utility>

#include "base/location.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_browsertest_util.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/task_management/task_management_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/guest_view_manager_factory.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/browser_plugin_guest_mode.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_speech_recognition_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/api/declarative/rules_registry.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative/test_rules_registry.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/test/extension_test_message_listener.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/events/event_switches.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/ppapi_test_utils.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/accessibility/speech_monitor.h"
#endif

using extensions::ContextMenuMatcher;
using extensions::ExtensionsAPIClient;
using extensions::MenuItem;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;
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
const char kUserAgentRedirectResponsePath[] = "/detect-user-agent";
const char kCacheResponsePath[] = "/cache-control-response";
const char kRedirectResponseFullPath[] =
    "/extensions/platform_apps/web_view/shim/guest_redirect.html";

class TestInterstitialPageDelegate : public content::InterstitialPageDelegate {
 public:
  TestInterstitialPageDelegate() {
  }
  ~TestInterstitialPageDelegate() override {}
  std::string GetHTMLContents() override { return std::string(); }
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
  void WasHidden() override {
    hidden_observed_ = true;
    hidden_callback_.Run();
  }

  bool hidden_observed() { return hidden_observed_; }

 private:
  base::Closure hidden_callback_;
  bool hidden_observed_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsHiddenObserver);
};

// Watches for context menu to be shown, records count of how many times
// context menu was shown.
class ContextMenuCallCountObserver {
 public:
  ContextMenuCallCountObserver()
      : num_times_shown_(0),
        menu_observer_(chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
                       base::Bind(&ContextMenuCallCountObserver::OnMenuShown,
                                  base::Unretained(this))) {
  }
  ~ContextMenuCallCountObserver() {}

  bool OnMenuShown(const content::NotificationSource& source,
                   const content::NotificationDetails& details) {
    ++num_times_shown_;
    auto context_menu = content::Source<RenderViewContextMenu>(source).ptr();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&RenderViewContextMenuBase::Cancel,
                              base::Unretained(context_menu)));
    return true;
  }

  void Wait() { menu_observer_.Wait(); }

  int num_times_shown() { return num_times_shown_; }

 private:
  int num_times_shown_;
  content::WindowedNotificationObserver menu_observer_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuCallCountObserver);
};

class EmbedderWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit EmbedderWebContentsObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents), terminated_(false) {}

  // WebContentsObserver.
  void RenderProcessGone(base::TerminationStatus status) override {
    terminated_ = true;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
  }

  void WaitForEmbedderRenderProcessTerminate() {
    if (terminated_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 private:
  bool terminated_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderWebContentsObserver);
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

#if defined (USE_AURA)
views::View* FindWebView(views::View* view) {
  std::queue<views::View*> queue;
  queue.push(view);
  while (!queue.empty()) {
    views::View* current = queue.front();
    queue.pop();
    if (std::string(current->GetClassName()).find("WebView") !=
        std::string::npos) {
      return current;
    }

    for (int i = 0; i < current->child_count(); ++i)
      queue.push(current->child_at(i));
  }
  return nullptr;
}
#endif

}  // namespace

// This class intercepts media access request from the embedder. The request
// should be triggered only if the embedder API (from tests) allows the request
// in Javascript.
// We do not issue the actual media request; the fact that the request reached
// embedder's WebContents is good enough for our tests. This is also to make
// the test run successfully on trybots.
class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate()
      : requested_(false),
        checked_(false) {}
  ~MockWebContentsDelegate() override {}

  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override {
    requested_ = true;
    if (request_message_loop_runner_.get())
      request_message_loop_runner_->Quit();
  }

  bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override {
    checked_ = true;
    if (check_message_loop_runner_.get())
      check_message_loop_runner_->Quit();
    return true;
  }

  void WaitForRequestMediaPermission() {
    if (requested_)
      return;
    request_message_loop_runner_ = new content::MessageLoopRunner;
    request_message_loop_runner_->Run();
  }

  void WaitForCheckMediaPermission() {
    if (checked_)
      return;
    check_message_loop_runner_ = new content::MessageLoopRunner;
    check_message_loop_runner_->Run();
  }

 private:
  bool requested_;
  bool checked_;
  scoped_refptr<content::MessageLoopRunner> request_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> check_message_loop_runner_;

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
  ~MockDownloadWebContentsDelegate() override {}

  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) override {
    orig_delegate_->CanDownload(
        url, request_method,
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

// TODO(wjmaclean): Fix this test class at some point so it can be re-enabled on
// the site isolation bots, and then look at re-enabling WebViewFocusTest when
// that happens.
// https://crbug.com/503751
class WebViewTestBase : public extensions::PlatformAppBrowserTest {
 protected:
  void SetUp() override {
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

  void TearDown() override {
    if (UsesFakeSpeech()) {
      // SpeechRecognition test specific TearDown.
      content::SpeechRecognitionManager::SetManagerForTesting(NULL);
    }

    extensions::PlatformAppBrowserTest::TearDown();
  }

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    // Mock out geolocation for geolocation specific tests.
    if (!strncmp(test_info->name(), "GeolocationAPI",
            strlen("GeolocationAPI"))) {
      ui_test_utils::OverrideGeolocation(10, 20);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
    command_line->AppendSwitchASCII(switches::kJavaScriptFlags, "--expose-gc");

    extensions::PlatformAppBrowserTest::SetUpCommandLine(command_line);
  }

  // Handles |request| by serving a redirect response if the |User-Agent| is
  // foobar.
  static std::unique_ptr<net::test_server::HttpResponse>
  UserAgentResponseHandler(const std::string& path,
                           const GURL& redirect_target,
                           const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    auto it = request.headers.find("User-Agent");
    EXPECT_TRUE(it != request.headers.end());
    if (!base::StartsWith("foobar", it->second,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_target.spec());
    return std::move(http_response);
  }

  // Handles |request| by serving a redirect response.
  static std::unique_ptr<net::test_server::HttpResponse>
  RedirectResponseHandler(const std::string& path,
                          const GURL& redirect_target,
                          const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
    http_response->AddCustomHeader("Location", redirect_target.spec());
    return std::move(http_response);
  }

  // Handles |request| by serving an empty response.
  static std::unique_ptr<net::test_server::HttpResponse> EmptyResponseHandler(
      const std::string& path,
      const net::test_server::HttpRequest& request) {
    if (base::StartsWith(path, request.relative_url,
                         base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>(
          new net::test_server::RawHttpResponse("", ""));

    return std::unique_ptr<net::test_server::HttpResponse>();
  }

  // Handles |request| by serving cache-able response.
  static std::unique_ptr<net::test_server::HttpResponse>
  CacheControlResponseHandler(const std::string& path,
                              const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(path, request.relative_url,
                          base::CompareCase::SENSITIVE))
      return std::unique_ptr<net::test_server::HttpResponse>();

    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->AddCustomHeader("Cache-control", "max-age=3600");
    http_response->set_content_type("text/plain");
    http_response->set_content("dummy text");
    return std::move(http_response);
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
      embedded_test_server()->RegisterRequestHandler(base::Bind(
          &WebViewTestBase::RedirectResponseHandler, kRedirectResponsePath,
          embedded_test_server()->GetURL(kRedirectResponseFullPath)));

      embedded_test_server()->RegisterRequestHandler(base::Bind(
          &WebViewTestBase::EmptyResponseHandler, kEmptyResponsePath));

      embedded_test_server()->RegisterRequestHandler(base::Bind(
          &WebViewTestBase::UserAgentResponseHandler,
          kUserAgentRedirectResponsePath,
          embedded_test_server()->GetURL(kRedirectResponseFullPath)));

      embedded_test_server()->RegisterRequestHandler(base::Bind(
          &WebViewTestBase::CacheControlResponseHandler, kCacheResponsePath));
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
    replace_host.SetHostStr("localhost");

    GURL guest_url = embedded_test_server()->GetURL(guest_path);
    guest_url = guest_url.ReplaceComponents(replace_host);

    ui_test_utils::UrlLoadObserver guest_observer(
        guest_url, content::NotificationService::AllSources());

    LoadAndLaunchPlatformApp(app_path.c_str(), "guest-loaded");

    guest_observer.Wait();
    content::Source<content::NavigationController> source =
        guest_observer.source();
    EXPECT_TRUE(source->GetWebContents()->GetRenderProcessHost()->
        IsForGuestsOnly());

    content::WebContents* guest_web_contents = source->GetWebContents();
    return guest_web_contents;
  }

  // Helper to load interstitial page in a <webview>.
  void InterstitialTeardownTestHelper() {
    // Start a HTTPS server so we can load an interstitial page inside guest.
    net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
    https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server.Start());

    net::HostPortPair host_and_port = https_server.host_port_pair();

    LoadAndLaunchPlatformApp("web_view/interstitial_teardown",
                             "EmbedderLoaded");

    // Now load the guest.
    content::WebContents* embedder_web_contents =
        GetFirstAppWindowWebContents();
    ExtensionTestMessageListener second("GuestAddedToDom", false);
    EXPECT_TRUE(content::ExecuteScript(
        embedder_web_contents,
        base::StringPrintf("loadGuest(%d);\n", host_and_port.port())));
    ASSERT_TRUE(second.WaitUntilSatisfied());

    // Wait for interstitial page to be shown in guest.
    content::WebContents* guest_web_contents =
        GetGuestViewManager()->WaitForSingleGuestCreated();
    ASSERT_TRUE(guest_web_contents->GetRenderProcessHost()->IsForGuestsOnly());
    content::WaitForInterstitialAttach(guest_web_contents);
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

  // Loads an app with a <webview> in it, returns once a guest is created.
  void LoadAppWithGuest(const std::string& app_path) {
    ExtensionTestMessageListener launched_listener("WebViewTest.LAUNCHED",
                                                   false);
    launched_listener.set_failure_message("WebViewTest.FAILURE");
    LoadAndLaunchPlatformApp(app_path.c_str(), &launched_listener);

    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
  }

  void SendMessageToEmbedder(const std::string& message) {
    EXPECT_TRUE(
        content::ExecuteScript(
            GetEmbedderWebContents(),
            base::StringPrintf("onAppCommand('%s');", message.c_str())));
  }

  void SendMessageToGuestAndWait(const std::string& message,
                                 const std::string& wait_message) {
    std::unique_ptr<ExtensionTestMessageListener> listener;
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

  void OpenContextMenu(content::WebContents* web_contents) {
    blink::WebMouseEvent mouse_event;
    mouse_event.type = blink::WebInputEvent::MouseDown;
    mouse_event.button = blink::WebMouseEvent::ButtonRight;
    mouse_event.x = 1;
    mouse_event.y = 1;
    web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        mouse_event);
    mouse_event.type = blink::WebInputEvent::MouseUp;
    web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
        mouse_event);
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

  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated may and will get called
    // before a guest is created.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  WebViewTestBase() : guest_web_contents_(NULL), embedder_web_contents_(NULL) {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~WebViewTestBase() override {}

 protected:
  scoped_refptr<content::FrameWatcher> frame_watcher_;

 private:
  bool UsesFakeSpeech() {
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();

    // SpeechRecognition test specific SetUp.
    const char* name = "SpeechRecognitionAPI_HasPermissionAllow";
    return !strncmp(test_info->name(), name, strlen(name));
  }

  std::unique_ptr<content::FakeSpeechRecognitionManager>
      fake_speech_recognition_manager_;

  TestGuestViewManagerFactory factory_;
  // Note that these are only set if you launch app using LoadAppWithGuest().
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;
};

class WebViewTest : public WebViewTestBase,
                    public testing::WithParamInterface<bool> {
 public:
  WebViewTest() {}
  ~WebViewTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTestBase::SetUpCommandLine(command_line);

    bool use_cross_process_frames_for_guests = GetParam();
    if (use_cross_process_frames_for_guests)
      command_line->AppendSwitch(switches::kUseCrossProcessFramesForGuests);
  }
};

INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewTest, testing::Bool());

class WebViewNewWindowTest : public WebViewTest {};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewNewWindowTest, testing::Bool());

class WebViewSizeTest : public WebViewTest {};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewSizeTest, testing::Bool());

class WebViewVisibilityTest : public WebViewTest {};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewVisibilityTest, testing::Bool());

class WebViewSpeechAPITest : public WebViewTest {};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewSpeechAPITest, testing::Bool());

// The following test suits are created to group tests based on specific
// features of <webview>.
// These features current would not work with
// --use-cross-process-frames-for-guest and is disabled on
// UseCrossProcessFramesForGuests.
class WebViewAccessibilityTest : public WebViewTest {};
INSTANTIATE_TEST_CASE_P(WebViewTests,
                        WebViewAccessibilityTest,
                        testing::Values(false));


class WebViewDPITest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", scale()));
  }

  static float scale() { return 2.0f; }
};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewDPITest, testing::Bool());

class WebViewWithZoomForDSFTest : public WebViewTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor,
                                    base::StringPrintf("%f", scale()));
    command_line->AppendSwitch(switches::kEnableUseZoomForDSF);
  }

  static float scale() { return 2.0f; }
};

class WebContentsAudioMutedObserver : public content::WebContentsObserver {
 public:
  explicit WebContentsAudioMutedObserver(content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        loop_runner_(new content::MessageLoopRunner),
        muting_update_observed_(false) {}

  // WebContentsObserver.
  void DidUpdateAudioMutingState(bool muted) override {
    muting_update_observed_ = true;
    loop_runner_->Quit();
  }

  void WaitForUpdate() {
    loop_runner_->Run();
  }

  bool muting_update_observed() { return muting_update_observed_; }

 private:
  scoped_refptr<content::MessageLoopRunner> loop_runner_;
  bool muting_update_observed_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAudioMutedObserver);
};

IN_PROC_BROWSER_TEST_P(WebViewTest, AudioMutesWhileAttached) {
  LoadAppWithGuest("web_view/simple");

  content::WebContents* embedder = GetEmbedderWebContents();
  content::WebContents* guest = GetGuestWebContents();

  EXPECT_FALSE(embedder->IsAudioMuted());
  EXPECT_FALSE(guest->IsAudioMuted());

  embedder->SetAudioMuted(true);
  EXPECT_TRUE(embedder->IsAudioMuted());
  EXPECT_TRUE(guest->IsAudioMuted());

  embedder->SetAudioMuted(false);
  EXPECT_FALSE(embedder->IsAudioMuted());
  EXPECT_FALSE(guest->IsAudioMuted());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, AudioMutesOnAttach) {
  LoadAndLaunchPlatformApp("web_view/app_creates_webview",
                           "WebViewTest.LAUNCHED");
  content::WebContents* embedder = GetEmbedderWebContents();
  embedder->SetAudioMuted(true);
  EXPECT_TRUE(embedder->IsAudioMuted());

  SendMessageToEmbedder("create-guest");
  content::WebContents* guest =
      GetGuestViewManager()->WaitForSingleGuestCreated();

  EXPECT_TRUE(embedder->IsAudioMuted());
  WebContentsAudioMutedObserver observer(guest);
  // If the guest hasn't attached yet, it may not have received the muting
  // update, in which case we should wait until it does.
  if (!guest->IsAudioMuted())
    observer.WaitForUpdate();
  EXPECT_TRUE(guest->IsAudioMuted());
}

// This test verifies that hiding the guest triggers WebContents::WasHidden().
IN_PROC_BROWSER_TEST_P(WebViewVisibilityTest, GuestVisibilityChanged) {
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
IN_PROC_BROWSER_TEST_P(WebViewVisibilityTest, EmbedderVisibilityChanged) {
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
IN_PROC_BROWSER_TEST_P(WebViewTest, ReloadEmbedder) {
  // Just load a guest from other test, we do not want to add a separate
  // platform_app for this test.
  LoadAppWithGuest("web_view/visibility_changed");

  ExtensionTestMessageListener launched_again_listener("WebViewTest.LAUNCHED",
                                                       false);
  GetEmbedderWebContents()->GetController().Reload(false);
  ASSERT_TRUE(launched_again_listener.WaitUntilSatisfied());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, AcceptTouchEvents) {
  // This test only makes sense for non-OOPIF WebView, since with
  // UseCrossProcessFramesForGuests() events are routed directly to the
  // guest, so the embedder does not need to know about the installation of
  // touch handlers.
  if (content::BrowserPluginGuestMode::UseCrossProcessFramesForGuests())
    return;

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
IN_PROC_BROWSER_TEST_P(WebViewTest, AddRemoveWebView_AddRemoveWebView) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/addremove"))
      << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewSizeTest, AutoSize) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/autosize"))
      << message_;
}

// Test for http://crbug.com/419611.
IN_PROC_BROWSER_TEST_P(WebViewTest, DisplayNoneSetSrc) {
  LoadAndLaunchPlatformApp("web_view/display_none_set_src",
                           "WebViewTest.LAUNCHED");
  // Navigate the guest while it's in "display: none" state.
  SendMessageToEmbedder("navigate-guest");
  GetGuestViewManager()->WaitForSingleGuestCreated();

  // Now attempt to navigate the guest again.
  SendMessageToEmbedder("navigate-guest");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED",
                                                    false);
  // Making the guest visible would trigger loadstop.
  SendMessageToEmbedder("show-guest");
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// Checks that {allFrames: true} injects script correctly to subframes
// inside <webview>.
IN_PROC_BROWSER_TEST_P(WebViewTest, ExecuteScript) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "execute_script")) << message_;
}

// http://crbug.com/326332
IN_PROC_BROWSER_TEST_P(WebViewSizeTest,
                       DISABLED_Shim_TestAutosizeAfterNavigation) {
  TestHelper("testAutosizeAfterNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAllowTransparencyAttribute) {
  TestHelper("testAllowTransparencyAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewDPITest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewWithZoomForDSFTest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewSizeTest, Shim_TestAutosizeHeight) {
  TestHelper("testAutosizeHeight", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewDPITest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewWithZoomForDSFTest,
                       Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewSizeTest, Shim_TestAutosizeBeforeNavigation) {
  TestHelper("testAutosizeBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewDPITest, Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewWithZoomForDSFTest,
                       Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewSizeTest, Shim_TestAutosizeRemoveAttributes) {
  TestHelper("testAutosizeRemoveAttributes", "web_view/shim", NO_TEST_SERVER);
}

// This test is disabled due to being flaky. http://crbug.com/282116
IN_PROC_BROWSER_TEST_P(WebViewSizeTest,
                       DISABLED_Shim_TestAutosizeWithPartialAttributes) {
  TestHelper("testAutosizeWithPartialAttributes",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAPIMethodExistence) {
  TestHelper("testAPIMethodExistence", "web_view/shim", NO_TEST_SERVER);
}

// Tests the existence of WebRequest API event objects on the request
// object, on the webview element, and hanging directly off webview.
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebRequestAPIExistence) {
  TestHelper("testWebRequestAPIExistence", "web_view/shim", NO_TEST_SERVER);
}

// Tests that addListener call succeeds on webview's WebRequest API events.
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebRequestAPIAddListener) {
  TestHelper("testWebRequestAPIAddListener", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebRequestAPIErrorOccurred) {
  TestHelper("testWebRequestAPIErrorOccurred", "web_view/shim", NO_TEST_SERVER);
}

// http://crbug.com/315920
#if defined(GOOGLE_CHROME_BUILD) && (defined(OS_WIN) || defined(OS_LINUX))
#define MAYBE_Shim_TestChromeExtensionURL DISABLED_Shim_TestChromeExtensionURL
#else
#define MAYBE_Shim_TestChromeExtensionURL Shim_TestChromeExtensionURL
#endif
IN_PROC_BROWSER_TEST_P(WebViewTest, MAYBE_Shim_TestChromeExtensionURL) {
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
IN_PROC_BROWSER_TEST_P(WebViewTest,
                       MAYBE_Shim_TestChromeExtensionRelativePath) {
  TestHelper("testChromeExtensionRelativePath",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestDisplayNoneWebviewLoad) {
  TestHelper("testDisplayNoneWebviewLoad", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestDisplayNoneWebviewRemoveChild) {
  // http://crbug.com/585652
  if (content::BrowserPluginGuestMode::UseCrossProcessFramesForGuests())
     return;
  TestHelper("testDisplayNoneWebviewRemoveChild",
             "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestDisplayBlock) {
  TestHelper("testDisplayBlock", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestInlineScriptFromAccessibleResources) {
  TestHelper("testInlineScriptFromAccessibleResources",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestInvalidChromeExtensionURL) {
  TestHelper("testInvalidChromeExtensionURL", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestEventName) {
  TestHelper("testEventName", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestOnEventProperty) {
  TestHelper("testOnEventProperties", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadProgressEvent) {
  TestHelper("testLoadProgressEvent", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestDestroyOnEventListener) {
  TestHelper("testDestroyOnEventListener", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestCannotMutateEventName) {
  TestHelper("testCannotMutateEventName", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestPartitionChangeAfterNavigation) {
  TestHelper("testPartitionChangeAfterNavigation",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestPartitionRemovalAfterNavigationFails) {
  TestHelper("testPartitionRemovalAfterNavigationFails",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAddContentScript) {
  TestHelper("testAddContentScript", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAddMultipleContentScripts) {
  TestHelper("testAddMultipleContentScripts", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    Shim_TestAddContentScriptWithSameNameShouldOverwriteTheExistingOne) {
  TestHelper("testAddContentScriptWithSameNameShouldOverwriteTheExistingOne",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    Shim_TestAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView) {
  TestHelper("testAddContentScriptToOneWebViewShouldNotInjectToTheOtherWebView",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAddAndRemoveContentScripts) {
  TestHelper("testAddAndRemoveContentScripts", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest,
                       Shim_TestAddContentScriptsWithNewWindowAPI) {
  TestHelper("testAddContentScriptsWithNewWindowAPI", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    Shim_TestContentScriptIsInjectedAfterTerminateAndReloadWebView) {
  TestHelper("testContentScriptIsInjectedAfterTerminateAndReloadWebView",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestContentScriptExistsAsLongAsWebViewTagExists) {
  TestHelper("testContentScriptExistsAsLongAsWebViewTagExists", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAddContentScriptWithCode) {
  TestHelper("testAddContentScriptWithCode", "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestExecuteScriptFail) {
  TestHelper("testExecuteScriptFail", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestExecuteScript) {
  TestHelper("testExecuteScript", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    Shim_TestExecuteScriptIsAbortedWhenWebViewSourceIsChanged) {
  TestHelper("testExecuteScriptIsAbortedWhenWebViewSourceIsChanged",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    Shim_TestExecuteScriptIsAbortedWhenWebViewSourceIsInvalid) {
  TestHelper("testExecuteScriptIsAbortedWhenWebViewSourceIsInvalid",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestTerminateAfterExit) {
  TestHelper("testTerminateAfterExit", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestAssignSrcAfterCrash) {
  TestHelper("testAssignSrcAfterCrash", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestNavOnConsecutiveSrcAttributeChanges) {
  TestHelper("testNavOnConsecutiveSrcAttributeChanges",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestNavOnSrcAttributeChange) {
  TestHelper("testNavOnSrcAttributeChange", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestNavigateAfterResize) {
  TestHelper("testNavigateAfterResize", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestNestedCrossOriginSubframes) {
  TestHelper("testNestedCrossOriginSubframes",
             "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestNestedSubframes) {
  TestHelper("testNestedSubframes", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestRemoveSrcAttribute) {
  TestHelper("testRemoveSrcAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestReassignSrcAttribute) {
  TestHelper("testReassignSrcAttribute", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest, Shim_TestNewWindow) {
  TestHelper("testNewWindow", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest, Shim_TestNewWindowTwoListeners) {
  TestHelper("testNewWindowTwoListeners", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest,
                       Shim_TestNewWindowNoPreventDefault) {
  TestHelper("testNewWindowNoPreventDefault",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest, Shim_TestNewWindowNoReferrerLink) {
  TestHelper("testNewWindowNoReferrerLink", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestContentLoadEvent) {
  TestHelper("testContentLoadEvent", "web_view/shim", NO_TEST_SERVER);
}

// TODO(fsamuel): Enable this test once <webview> can run in a detached state.
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestContentLoadEventWithDisplayNone) {
  TestHelper("testContentLoadEventWithDisplayNone",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestDeclarativeWebRequestAPI) {
  TestHelper("testDeclarativeWebRequestAPI",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestDeclarativeWebRequestAPISendMessage) {
  TestHelper("testDeclarativeWebRequestAPISendMessage",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebRequestAPI) {
  TestHelper("testWebRequestAPI", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebRequestAPIWithHeaders) {
  TestHelper("testWebRequestAPIWithHeaders",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebRequestAPIGoogleProperty) {
  TestHelper("testWebRequestAPIGoogleProperty",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestWebRequestListenerSurvivesReparenting) {
  TestHelper("testWebRequestListenerSurvivesReparenting",
             "web_view/shim",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadStartLoadRedirect) {
  TestHelper("testLoadStartLoadRedirect", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestLoadAbortChromeExtensionURLWrongPartition) {
  TestHelper("testLoadAbortChromeExtensionURLWrongPartition",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadAbortEmptyResponse) {
  TestHelper("testLoadAbortEmptyResponse", "web_view/shim", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadAbortIllegalChromeURL) {
  TestHelper("testLoadAbortIllegalChromeURL",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadAbortIllegalFileURL) {
  TestHelper("testLoadAbortIllegalFileURL", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadAbortIllegalJavaScriptURL) {
  TestHelper("testLoadAbortIllegalJavaScriptURL",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadAbortInvalidNavigation) {
  TestHelper("testLoadAbortInvalidNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadAbortNonWebSafeScheme) {
  TestHelper("testLoadAbortNonWebSafeScheme", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestReload) {
  TestHelper("testReload", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestReloadAfterTerminate) {
  TestHelper("testReloadAfterTerminate", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestGetProcessId) {
  TestHelper("testGetProcessId", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewVisibilityTest, Shim_TestHiddenBeforeNavigation) {
  TestHelper("testHiddenBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestRemoveWebviewOnExit) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.

  // Launch the app and wait until it's ready to load a test.
  LoadAndLaunchPlatformApp("web_view/shim", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");

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
      IsForGuestsOnly());

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
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestRemoveWebviewAfterNavigation) {
  TestHelper("testRemoveWebviewAfterNavigation",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestNavigationToExternalProtocol) {
  TestHelper("testNavigationToExternalProtocol",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewSizeTest,
                       Shim_TestResizeWebviewWithDisplayNoneResizesContent) {
  TestHelper("testResizeWebviewWithDisplayNoneResizesContent",
             "web_view/shim",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewSizeTest, Shim_TestResizeWebviewResizesContent) {
  TestHelper("testResizeWebviewResizesContent",
             "web_view/shim",
             NO_TEST_SERVER);
}

// This test makes sure the browser process does not crash if app is closed
// while an interstitial page is being shown in guest.
IN_PROC_BROWSER_TEST_P(WebViewTest, InterstitialTeardown) {
  InterstitialTeardownTestHelper();

  // Now close the app while interstitial page being shown in guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();
}

// This test makes sure the browser process does not crash if browser is shut
// down while an interstitial page is being shown in guest.
IN_PROC_BROWSER_TEST_P(WebViewTest, InterstitialTeardownOnBrowserShutdown) {
  InterstitialTeardownTestHelper();

  // Now close the app while interstitial page being shown in guest.
  extensions::AppWindow* window = GetFirstAppWindow();
  window->GetBaseWindow()->Close();

  // InterstitialPage is not destroyed immediately, so the
  // RenderWidgetHostViewGuest for it is still there, closing all
  // renderer processes will cause the RWHVGuest's RenderProcessGone()
  // shutdown path to be exercised.
  chrome::CloseAllBrowsers();
}

IN_PROC_BROWSER_TEST_P(WebViewTest, ShimSrcAttribute) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/src_attribute"))
      << message_;
}

// This test verifies that prerendering has been disabled inside <webview>.
// This test is here rather than in PrerenderBrowserTest for testing convenience
// only. If it breaks then this is a bug in the prerenderer.
IN_PROC_BROWSER_TEST_P(WebViewTest, NoPrerenderer) {
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
IN_PROC_BROWSER_TEST_P(WebViewTest, TaskManagerExistingWebView) {
  // This test is for the old implementation of the task manager. We must
  // explicitly disable the new one.
  task_manager::browsertest_util::EnableOldTaskManager();

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
IN_PROC_BROWSER_TEST_P(WebViewTest, TaskManagerNewWebView) {
  // This test is for the old implementation of the task manager. We must
  // explicitly disable the new one.
  task_manager::browsertest_util::EnableOldTaskManager();

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
IN_PROC_BROWSER_TEST_P(WebViewTest, CookieIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // Navigate the browser to a page which writes a sample cookie
  // The cookie is "testCookie=1"
  GURL set_cookie_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/cookie_isolation/set_cookie.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  set_cookie_url = set_cookie_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), set_cookie_url);
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/cookie_isolation"))
      << message_;
  // Finally, verify that the browser cookie has not changed.
  int cookie_size;
  std::string cookie_value;

  ui_test_utils::GetCookies(GURL("http://localhost"),
                            browser()->tab_strip_model()->GetWebContentsAt(0),
                            &cookie_size, &cookie_value);
  EXPECT_EQ("testCookie=1", cookie_value);
}

// This tests that in-memory storage partitions are reset on browser restart,
// but persistent ones maintain state for cookies and HTML5 storage.
IN_PROC_BROWSER_TEST_P(WebViewTest, PRE_StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // We don't care where the main browser is on this test.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  // Start the app for the pre-test.
  LoadAndLaunchPlatformApp("web_view/storage_persistence",
                           "WebViewTest.LAUNCHED");

  // Send a message to run the PRE_StoragePersistence part of the test.
  SendMessageToEmbedder("run-pre-test");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED",
                                                    false);
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// This is the post-reset portion of the StoragePersistence test.  See
// PRE_StoragePersistence for main comment.
IN_PROC_BROWSER_TEST_P(WebViewTest, StoragePersistence) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  // We don't care where the main browser is on this test.
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
  // Start the app for the pre-test.
  LoadAndLaunchPlatformApp("web_view/storage_persistence",
                           "WebViewTest.LAUNCHED");

  // Send a message to run the StoragePersistence part of the test.
  SendMessageToEmbedder("run-test");

  ExtensionTestMessageListener test_passed_listener("WebViewTest.PASSED",
                                                    false);
  EXPECT_TRUE(test_passed_listener.WaitUntilSatisfied());
}

// This tests DOM storage isolation for packaged apps with webview tags. It
// loads an app with multiple webview tags and each tag sets DOM storage
// entries, which the test checks to ensure proper storage isolation is
// enforced.
IN_PROC_BROWSER_TEST_P(WebViewTest, DOMStorageIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL navigate_to_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/web_view/dom_storage_isolation/page.html");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  navigate_to_url = navigate_to_url.ReplaceComponents(replace_host);

  ui_test_utils::NavigateToURL(browser(), navigate_to_url);
  ASSERT_TRUE(
      RunPlatformAppTest("platform_apps/web_view/dom_storage_isolation"));
  // Verify that the browser tab's local/session storage does not have the same
  // values which were stored by the webviews.
  std::string output;
  std::string get_local_storage(
      "window.domAutomationController.send("
      "window.localStorage.getItem('foo') || 'badval')");
  std::string get_session_storage(
      "window.domAutomationController.send("
      "window.localStorage.getItem('baz') || 'badval')");
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_local_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
  ASSERT_TRUE(ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      get_session_storage.c_str(), &output));
  EXPECT_STREQ("badval", output.c_str());
}

// This tests IndexedDB isolation for packaged apps with webview tags. It loads
// an app with multiple webview tags and each tag creates an IndexedDB record,
// which the test checks to ensure proper storage isolation is enforced.
IN_PROC_BROWSER_TEST_P(WebViewTest, IndexedDBIsolation) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunPlatformAppTest(
      "platform_apps/web_view/isolation_indexeddb")) << message_;
}

// This test ensures that closing app window on 'loadcommit' does not crash.
// The test launches an app with guest and closes the window on loadcommit. It
// then launches the app window again. The process is repeated 3 times.
// http://crbug.com/291278
#if defined(OS_WIN) || defined(MAC_OS_X_VERSION_10_9)
#define MAYBE_CloseOnLoadcommit DISABLED_CloseOnLoadcommit
#else
#define MAYBE_CloseOnLoadcommit CloseOnLoadcommit
#endif
IN_PROC_BROWSER_TEST_P(WebViewTest, MAYBE_CloseOnLoadcommit) {
  LoadAndLaunchPlatformApp("web_view/close_on_loadcommit",
                           "done-close-on-loadcommit");
}

IN_PROC_BROWSER_TEST_P(WebViewTest, MediaAccessAPIDeny_TestDeny) {
  MediaAccessAPIDenyTestHelper("testDeny");
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       MediaAccessAPIDeny_TestDenyThenAllowThrows) {
  MediaAccessAPIDenyTestHelper("testDenyThenAllowThrows");
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       MediaAccessAPIDeny_TestDenyWithPreventDefault) {
  MediaAccessAPIDenyTestHelper("testDenyWithPreventDefault");
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       MediaAccessAPIDeny_TestNoListenersImplyDeny) {
  MediaAccessAPIDenyTestHelper("testNoListenersImplyDeny");
}

IN_PROC_BROWSER_TEST_P(WebViewTest,
                       MediaAccessAPIDeny_TestNoPreventDefaultImpliesDeny) {
  MediaAccessAPIDenyTestHelper("testNoPreventDefaultImpliesDeny");
}

void WebViewTestBase::MediaAccessAPIAllowTestHelper(
    const std::string& test_name) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/media_access/allow", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents->SetDelegate(mock.get());

  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(
      content::ExecuteScript(
          embedder_web_contents,
          base::StringPrintf("startAllowTest('%s')",
                             test_name.c_str())));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForRequestMediaPermission();
}

IN_PROC_BROWSER_TEST_P(WebViewTest, OpenURLFromTab_CurrentTab_Abort) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of CURRENT_TAB will
  // navigate the current <webview>.
  ExtensionTestMessageListener load_listener("WebViewTest.LOADSTOP", false);

  // Navigating to a file URL is forbidden inside a <webview>.
  content::OpenURLParams params(GURL("file://foo"),
                                content::Referrer(),
                                CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                true /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params);

  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Verify that the <webview> ends up at about:blank.
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            GetGuestWebContents()->GetLastCommittedURL());
}

// A navigation to a web-safe URL should succeed, even if it is not renderer-
// initiated, such as a navigation from the PDF viewer.
IN_PROC_BROWSER_TEST_P(WebViewTest, OpenURLFromTab_CurrentTab_Succeed) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of CURRENT_TAB will
  // navigate the current <webview>.
  ExtensionTestMessageListener load_listener("WebViewTest.LOADSTOP", false);

  GURL test_url("http://www.google.com");
  content::OpenURLParams params(test_url, content::Referrer(), CURRENT_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                false /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(GetGuestWebContents(),
                                                       params);

  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  EXPECT_EQ(test_url, GetGuestWebContents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest, OpenURLFromTab_NewWindow_Abort) {
  LoadAppWithGuest("web_view/simple");

  // Verify that OpenURLFromTab with a window disposition of NEW_BACKGROUND_TAB
  // will trigger the <webview>'s New Window API.
  ExtensionTestMessageListener new_window_listener(
      "WebViewTest.NEWWINDOW", false);

  // Navigating to a file URL is forbidden inside a <webview>.
  content::OpenURLParams params(GURL("file://foo"),
                                content::Referrer(),
                                NEW_BACKGROUND_TAB,
                                ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                true /* is_renderer_initiated */);
  GetGuestWebContents()->GetDelegate()->OpenURLFromTab(
      GetGuestWebContents(), params);

  ASSERT_TRUE(new_window_listener.WaitUntilSatisfied());

  // Verify that a new guest was created.
  content::WebContents* new_guest_web_contents =
      GetGuestViewManager()->GetLastGuestCreated();
  EXPECT_NE(GetGuestWebContents(), new_guest_web_contents);

  // Verify that the new <webview> guest ends up at about:blank.
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            new_guest_web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, ContextMenuInspectElement) {
  LoadAppWithGuest("web_view/context_menus/basic");
  content::WebContents* guest_web_contents = GetGuestWebContents();
  ASSERT_TRUE(guest_web_contents);

  content::ContextMenuParams params;
  TestRenderViewContextMenu menu(guest_web_contents->GetMainFrame(), params);
  menu.Init();

  // Expect "Inspect" to be shown as we are running webview in a chrome app.
  EXPECT_TRUE(menu.IsItemPresent(IDC_CONTENT_CONTEXT_INSPECTELEMENT));
}

// This test executes the context menu command 'LanguageSettings' which will
// load chrome://settings/languages in a browser window. This is a browser-
// initiated operation and so we expect this to succeed if the embedder is
// allowed to perform the operation.
IN_PROC_BROWSER_TEST_P(WebViewTest, ContextMenuLanguageSettings) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // Create and build our test context menu.
  content::WebContentsAddedObserver web_contents_added_observer;

  GURL page_url("http://www.google.com");
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(guest_web_contents, page_url, GURL(),
                                        GURL()));
  menu->ExecuteCommand(IDC_CONTENT_CONTEXT_LANGUAGE_SETTINGS, 0);

  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();

  // Verify that a new WebContents has been created that is at the Language
  // Settings page.
  EXPECT_EQ(GURL("chrome://settings/languages"),
            new_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, ContextMenusAPI_Basic) {
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
  std::unique_ptr<TestRenderViewContextMenu> menu(
      TestRenderViewContextMenu::Create(guest_web_contents, page_url, GURL(),
                                        GURL()));
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

// Called in the TestContextMenu test to cancel the context menu after its
// shown notification is received.
static bool ContextMenuNotificationCallback(
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  auto context_menu = content::Source<RenderViewContextMenu>(source).ptr();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&RenderViewContextMenuBase::Cancel,
                            base::Unretained(context_menu)));
  return true;
}

IN_PROC_BROWSER_TEST_P(WebViewTest, ContextMenusAPI_PreventDefault) {
  LoadAppWithGuest("web_view/context_menus/basic");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::WebContents* embedder = GetEmbedderWebContents();
  ASSERT_TRUE(embedder);

  // Add a preventDefault() call on context menu event so context menu
  // does not show up.
  ExtensionTestMessageListener prevent_default_listener(
      "WebViewTest.CONTEXT_MENU_DEFAULT_PREVENTED", false);
  EXPECT_TRUE(content::ExecuteScript(embedder, "registerPreventDefault()"));
  ContextMenuCallCountObserver context_menu_shown_observer;

  OpenContextMenu(guest_web_contents);

  EXPECT_TRUE(prevent_default_listener.WaitUntilSatisfied());
  // Expect the menu to not show up.
  EXPECT_EQ(0, context_menu_shown_observer.num_times_shown());

  // Now remove the preventDefault() and expect context menu to be shown.
  ExecuteScriptWaitForTitle(
      embedder, "removePreventDefault()", "PREVENT_DEFAULT_LISTENER_REMOVED");
  OpenContextMenu(guest_web_contents);

  // We expect to see a context menu for the second call to |OpenContextMenu|.
  context_menu_shown_observer.Wait();
  EXPECT_EQ(1, context_menu_shown_observer.num_times_shown());
}

// Tests that a context menu is created when right-clicking in the webview. This
// also tests that the 'contextmenu' event is handled correctly.
IN_PROC_BROWSER_TEST_P(WebViewTest, TestContextMenu) {
  LoadAppWithGuest("web_view/context_menus/basic");
  content::WebContents* guest_web_contents = GetGuestWebContents();

  // Register an observer for the context menu.
  content::WindowedNotificationObserver menu_observer(
      chrome::NOTIFICATION_RENDER_VIEW_CONTEXT_MENU_SHOWN,
      base::Bind(ContextMenuNotificationCallback));

  OpenContextMenu(guest_web_contents);

  // Wait for the context menu to be visible.
  menu_observer.Wait();
}

IN_PROC_BROWSER_TEST_P(WebViewTest, MediaAccessAPIAllow_TestAllow) {
  MediaAccessAPIAllowTestHelper("testAllow");
}

IN_PROC_BROWSER_TEST_P(WebViewTest, MediaAccessAPIAllow_TestAllowAndThenDeny) {
  MediaAccessAPIAllowTestHelper("testAllowAndThenDeny");
}

IN_PROC_BROWSER_TEST_P(WebViewTest, MediaAccessAPIAllow_TestAllowTwice) {
  MediaAccessAPIAllowTestHelper("testAllowTwice");
}

IN_PROC_BROWSER_TEST_P(WebViewTest, MediaAccessAPIAllow_TestAllowAsync) {
  MediaAccessAPIAllowTestHelper("testAllowAsync");
}

IN_PROC_BROWSER_TEST_P(WebViewTest, MediaAccessAPIAllow_TestCheck) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  LoadAndLaunchPlatformApp("web_view/media_access/check", "Launched");

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<MockWebContentsDelegate> mock(new MockWebContentsDelegate());
  embedder_web_contents->SetDelegate(mock.get());

  ExtensionTestMessageListener done_listener("TEST_PASSED", false);
  done_listener.set_failure_message("TEST_FAILED");
  EXPECT_TRUE(
      content::ExecuteScript(
          embedder_web_contents,
          base::StringPrintf("startCheckTest('')")));
  ASSERT_TRUE(done_listener.WaitUntilSatisfied());

  mock->WaitForCheckMediaPermission();
}

// Checks that window.screenX/screenY/screenLeft/screenTop works correctly for
// guests.
IN_PROC_BROWSER_TEST_P(WebViewTest, ScreenCoordinates) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "screen_coordinates"))
          << message_;
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_P(WebViewTest, ChromeVoxInjection) {
  EXPECT_FALSE(
      chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled());

  chromeos::SpeechMonitor monitor;
  chromeos::AccessibilityManager::Get()->EnableSpokenFeedback(
      true, ui::A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(monitor.SkipChromeVoxEnabledMessage());

  ASSERT_TRUE(StartEmbeddedTestServer());
  content::WebContents* guest_web_contents = LoadGuest(
      "/extensions/platform_apps/web_view/chromevox_injection/guest.html",
      "web_view/chromevox_injection");
  ASSERT_TRUE(guest_web_contents);

  for (;;) {
    if (monitor.GetNextUtterance() == "chrome vox test title")
      break;
  }
}
#endif

// Flaky on Windows. http://crbug.com/303966
#if defined(OS_WIN)
#define MAYBE_TearDownTest DISABLED_TearDownTest
#else
#define MAYBE_TearDownTest TearDownTest
#endif
IN_PROC_BROWSER_TEST_P(WebViewTest, MAYBE_TearDownTest) {
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("web_view/simple", "WebViewTest.LAUNCHED");
  extensions::AppWindow* window = NULL;
  if (!GetAppWindowCount())
    window = CreateAppWindow(browser()->profile(), extension);
  else
    window = GetFirstAppWindow();
  CloseAppWindow(window);

  // Load the app again.
  LoadAndLaunchPlatformApp("web_view/simple", "WebViewTest.LAUNCHED");
}

// In following GeolocationAPIEmbedderHasNoAccess* tests, embedder (i.e. the
// platform app) does not have geolocation permission for this test.
// No matter what the API does, geolocation permission would be denied.
// Note that the test name prefix must be "GeolocationAPI".
IN_PROC_BROWSER_TEST_P(WebViewTest, GeolocationAPIEmbedderHasNoAccessAllow) {
  TestHelper("testDenyDenies",
             "web_view/geolocation/embedder_has_no_permission",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, GeolocationAPIEmbedderHasNoAccessDeny) {
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
//
// GeolocationAPI* test 1 of 3.
// Currently disabled until crbug.com/526788 is fixed.
IN_PROC_BROWSER_TEST_P(WebViewTest,
                       DISABLED_GeolocationAPIEmbedderHasAccessAllow) {
  TestHelper("testAllow",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// GeolocationAPI* test 2 of 3.
IN_PROC_BROWSER_TEST_P(WebViewTest, GeolocationAPIEmbedderHasAccessDeny) {
  TestHelper("testDeny",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// GeolocationAPI* test 3 of 3.
// Currently disabled until crbug.com/526788 is fixed.
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    DISABLED_GeolocationAPIEmbedderHasAccessMultipleBridgeIdAllow) {
  TestHelper("testMultipleBridgeIdAllow",
             "web_view/geolocation/embedder_has_permission",
             NEEDS_TEST_SERVER);
}

// Tests that
// BrowserPluginGeolocationPermissionContext::CancelGeolocationPermissionRequest
// is handled correctly (and does not crash).
IN_PROC_BROWSER_TEST_P(WebViewTest, GeolocationAPICancelGeolocation) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest(
        "platform_apps/web_view/geolocation/cancel_request")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewTest, DISABLED_GeolocationRequestGone) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest(
        "platform_apps/web_view/geolocation/geolocation_request_gone"))
            << message_;
}

// In following FilesystemAPIRequestFromMainThread* tests, guest request
// filesystem access from main thread of the guest.
// FileSystemAPIRequestFromMainThread* test 1 of 3
IN_PROC_BROWSER_TEST_P(WebViewTest, FileSystemAPIRequestFromMainThreadAllow) {
  TestHelper("testAllow", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromMainThread* test 2 of 3.
IN_PROC_BROWSER_TEST_P(WebViewTest, FileSystemAPIRequestFromMainThreadDeny) {
  TestHelper("testDeny", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromMainThread* test 3 of 3.
IN_PROC_BROWSER_TEST_P(WebViewTest,
                       FileSystemAPIRequestFromMainThreadDefaultAllow) {
  TestHelper("testDefaultAllow", "web_view/filesystem/main", NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromWorker* tests, guest create a worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromWorker* test 1 of 3
IN_PROC_BROWSER_TEST_P(WebViewTest, FileSystemAPIRequestFromWorkerAllow) {
  TestHelper("testAllow", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromWorker* test 2 of 3.
IN_PROC_BROWSER_TEST_P(WebViewTest, FileSystemAPIRequestFromWorkerDeny) {
  TestHelper("testDeny", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromWorker* test 3 of 3.
IN_PROC_BROWSER_TEST_P(WebViewTest,
                       FileSystemAPIRequestFromWorkerDefaultAllow) {
  TestHelper(
      "testDefaultAllow", "web_view/filesystem/worker", NEEDS_TEST_SERVER);
}

// In following FilesystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* tests,
// embedder contains a single webview guest. The guest creates a shared worker
// to request filesystem access from worker thread.
// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 1 of 3
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestAllow) {
  TestHelper("testAllow",
             "web_view/filesystem/shared_worker/single",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 2 of 3.
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuestDeny) {
  TestHelper("testDeny",
             "web_view/filesystem/shared_worker/single",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfSingleWebViewGuest* test 3 of 3.
IN_PROC_BROWSER_TEST_P(
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
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsAllow) {
  TestHelper("testAllow",
             "web_view/filesystem/shared_worker/multiple",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 2 of 3.
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsDeny) {
  TestHelper("testDeny",
             "web_view/filesystem/shared_worker/multiple",
             NEEDS_TEST_SERVER);
}

// FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuests* test 3 of 3.
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    FileSystemAPIRequestFromSharedWorkerOfMultiWebViewGuestsDefaultAllow) {
  TestHelper(
      "testDefaultAllow",
      "web_view/filesystem/shared_worker/multiple",
      NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, ClearData) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "cleardata"))
          << message_;
}

#if defined(OS_WIN)
// Test is disabled on Windows because it fails often (~9% time)
// http://crbug.com/489088
#define MAYBE_ClearDataCache DISABLED_ClearDataCache
#else
#define MAYBE_ClearDataCache ClearDataCache
#endif
IN_PROC_BROWSER_TEST_P(WebViewTest, MAYBE_ClearDataCache) {
  TestHelper("testClearCache", "web_view/clear_data_cache", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, ConsoleMessage) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/web_view/common", "console_messages"))
          << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewTest, DownloadPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  content::WebContents* guest_web_contents =
      LoadGuest("/extensions/platform_apps/web_view/download/guest.html",
                "web_view/download");
  ASSERT_TRUE(guest_web_contents);

  // Replace WebContentsDelegate with mock version so we can intercept download
  // requests.
  content::WebContentsDelegate* delegate = guest_web_contents->GetDelegate();
  std::unique_ptr<MockDownloadWebContentsDelegate> mock_delegate(
      new MockDownloadWebContentsDelegate(delegate));
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
IN_PROC_BROWSER_TEST_P(WebViewTest, WhitelistedContentScript) {
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

IN_PROC_BROWSER_TEST_P(WebViewTest, SendMessageToExtensionFromGuest) {
  // Load the extension as a normal, non-component extension.
  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII(
          "platform_apps/web_view/extension_api/component_extension"));
  ASSERT_TRUE(extension);

  TestHelper("testNonComponentExtension", "web_view/component_extension",
             NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, SendMessageToComponentExtensionFromGuest) {
  const extensions::Extension* component_extension =
      LoadExtensionAsComponent(test_data_dir_.AppendASCII(
          "platform_apps/web_view/extension_api/component_extension"));
  ASSERT_TRUE(component_extension);

  TestHelper("testComponentExtension", "web_view/component_extension",
             NEEDS_TEST_SERVER);

  content::WebContents* embedder_web_contents = GetFirstAppWindowWebContents();
  ASSERT_TRUE(embedder_web_contents);

  // Retrive the guestProcessId and guestRenderFrameRoutingId from the
  // extension.
  int guest_process_id = content::ChildProcessHost::kInvalidUniqueID;
  content::ExecuteScriptAndGetValue(embedder_web_contents->GetMainFrame(),
                                    "window.guestProcessId")
      ->GetAsInteger(&guest_process_id);
  int guest_render_frame_routing_id = MSG_ROUTING_NONE;
  content::ExecuteScriptAndGetValue(embedder_web_contents->GetMainFrame(),
                                    "window.guestRenderFrameRoutingId")
      ->GetAsInteger(&guest_render_frame_routing_id);

  auto* guest_rfh = content::RenderFrameHost::FromID(
      guest_process_id, guest_render_frame_routing_id);
  // Verify that the guest related info (guest_process_id and
  // guest_render_frame_routing_id) actually points to a WebViewGuest.
  ASSERT_TRUE(extensions::WebViewGuest::FromWebContents(
      content::WebContents::FromRenderFrameHost(guest_rfh)));
}

IN_PROC_BROWSER_TEST_P(WebViewTest, SetPropertyOnDocumentReady) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/document_ready"))
                  << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewTest, SetPropertyOnDocumentInteractive) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/document_interactive"))
                  << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_HasPermissionAllow) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/speech_recognition_api",
                                "allowTest"))
          << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_HasPermissionDeny) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/speech_recognition_api",
                                "denyTest"))
          << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewSpeechAPITest,
                       SpeechRecognitionAPI_NoPermission) {
  ASSERT_TRUE(
      RunPlatformAppTestWithArg("platform_apps/web_view/common",
                                "speech_recognition_api_no_permission"))
          << message_;
}

// Tests overriding user agent.
IN_PROC_BROWSER_TEST_P(WebViewTest, UserAgent) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
              "platform_apps/web_view/common", "useragent")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewNewWindowTest, UserAgent_NewWindow) {
  ASSERT_TRUE(RunPlatformAppTestWithArg(
              "platform_apps/web_view/common",
              "useragent_newwindow")) << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewTest, NoPermission) {
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/nopermission"))
                  << message_;
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Dialog_TestAlertDialog) {
  TestHelper("testAlertDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, TestConfirmDialog) {
  TestHelper("testConfirmDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Dialog_TestConfirmDialogCancel) {
  TestHelper("testConfirmDialogCancel", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Dialog_TestConfirmDialogDefaultCancel) {
  TestHelper("testConfirmDialogDefaultCancel",
             "web_view/dialog",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Dialog_TestConfirmDialogDefaultGCCancel) {
  TestHelper("testConfirmDialogDefaultGCCancel",
             "web_view/dialog",
             NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Dialog_TestPromptDialog) {
  TestHelper("testPromptDialog", "web_view/dialog", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, NoContentSettingsAPI) {
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
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);
    ASSERT_TRUE(ppapi::RegisterTestPlugin(command_line));
  }
};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewPluginTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(WebViewPluginTest, TestLoadPluginEvent) {
  TestHelper("testPluginLoadPermission", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewPluginTest, TestLoadPluginInternalResource) {
  const char kTestMimeType[] = "application/pdf";
  const char kTestFileType[] = "pdf";
  content::WebPluginInfo plugin_info;
  plugin_info.type = content::WebPluginInfo::PLUGIN_TYPE_PEPPER_OUT_OF_PROCESS;
  plugin_info.mime_types.push_back(
      content::WebPluginMimeType(kTestMimeType, kTestFileType, std::string()));
  content::PluginService::GetInstance()->RegisterInternalPlugin(plugin_info,
                                                                true);

  TestHelper("testPluginLoadInternalResource", "web_view/shim", NO_TEST_SERVER);
}
#endif  // defined(ENABLE_PLUGINS)

class WebViewCaptureTest : public WebViewTest {
 public:
  WebViewCaptureTest() {}
  ~WebViewCaptureTest() override {}
  void SetUp() override {
    EnablePixelOutput();
    WebViewTest::SetUp();
  }
};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewCaptureTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestZoomAPI) {
  TestHelper("testZoomAPI", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestFindAPI) {
  TestHelper("testFindAPI", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestFindAPI_findupdate) {
  TestHelper("testFindAPI_findupdate", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestLoadDataAPI) {
  TestHelper("testLoadDataAPI", "web_view/shim", NEEDS_TEST_SERVER);
}

// This test verifies that the resize and contentResize events work correctly.
IN_PROC_BROWSER_TEST_P(WebViewSizeTest, Shim_TestResizeEvents) {
  TestHelper("testResizeEvents", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestPerOriginZoomMode) {
  TestHelper("testPerOriginZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestPerViewZoomMode) {
  TestHelper("testPerViewZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestDisabledZoomMode) {
  TestHelper("testDisabledZoomMode", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestZoomBeforeNavigation) {
  TestHelper("testZoomBeforeNavigation", "web_view/shim", NO_TEST_SERVER);
}

// This test verify that the set of rules registries of a webview will be
// removed from RulesRegistryService after the webview is gone.
// http://crbug.com/438327
IN_PROC_BROWSER_TEST_P(
    WebViewTest,
    DISABLED_Shim_TestRulesRegistryIDAreRemovedAfterWebViewIsGone) {
  LoadAppWithGuest("web_view/rules_registry");

  content::WebContents* embedder_web_contents = GetEmbedderWebContents();
  ASSERT_TRUE(embedder_web_contents);
  std::unique_ptr<EmbedderWebContentsObserver> observer(
      new EmbedderWebContentsObserver(embedder_web_contents));

  content::WebContents* guest_web_contents = GetGuestWebContents();
  ASSERT_TRUE(guest_web_contents);
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromWebContents(guest_web_contents);
  ASSERT_TRUE(guest);

  // Register rule for the guest.
  Profile* profile = browser()->profile();
  int rules_registry_id =
      extensions::WebViewGuest::GetOrGenerateRulesRegistryID(
          guest->owner_web_contents()->GetRenderProcessHost()->GetID(),
          guest->view_instance_id());

  extensions::RulesRegistryService* registry_service =
      extensions::RulesRegistryService::Get(profile);
  extensions::TestRulesRegistry* rules_registry =
      new extensions::TestRulesRegistry(
          content::BrowserThread::UI, "ui", rules_registry_id);
  registry_service->RegisterRulesRegistry(make_scoped_refptr(rules_registry));

  EXPECT_TRUE(registry_service->GetRulesRegistry(
      rules_registry_id, "ui").get());

  // Kill the embedder's render process, so the webview will go as well.
  base::Process process = base::Process::DeprecatedGetProcessFromHandle(
        embedder_web_contents->GetRenderProcessHost()->GetHandle());
  process.Terminate(0, false);
  observer->WaitForEmbedderRenderProcessTerminate();

  EXPECT_FALSE(registry_service->GetRulesRegistry(
      rules_registry_id, "ui").get());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_WebViewWebRequestRegistryHasNoCache) {
  LoadAppWithGuest("web_view/rules_registry");

  content::WebContents* guest_web_contents = GetGuestWebContents();
  ASSERT_TRUE(guest_web_contents);
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromWebContents(guest_web_contents);
  ASSERT_TRUE(guest);

  Profile* profile = browser()->profile();
  extensions::RulesRegistryService* registry_service =
      extensions::RulesRegistryService::Get(profile);
  int rules_registry_id =
      extensions::WebViewGuest::GetOrGenerateRulesRegistryID(
          guest->owner_web_contents()->GetRenderProcessHost()->GetID(),
          guest->view_instance_id());

  // Get an existing registered rule for the guest.
  extensions::RulesRegistry* registry =
      registry_service->GetRulesRegistry(
          rules_registry_id,
          extensions::declarative_webrequest_constants::kOnRequest).get();

  EXPECT_TRUE(registry);
  EXPECT_FALSE(registry->rules_cache_delegate_for_testing());
}

// This test verifies that webview.contentWindow works inside an iframe.
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestWebViewInsideFrame) {
  LoadAppWithGuest("web_view/inside_iframe");
}

// <webview> screenshot capture fails with ubercomp.
// See http://crbug.com/327035.
IN_PROC_BROWSER_TEST_P(WebViewCaptureTest, DISABLED_Shim_ScreenshotCapture) {
  TestHelper("testScreenshotCapture", "web_view/shim", NO_TEST_SERVER);
}

// Tests that browser process does not crash when loading plugin inside
// <webview> with content settings set to CONTENT_SETTING_BLOCK.
IN_PROC_BROWSER_TEST_P(WebViewTest, TestPlugin) {
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
    ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                               CONTENT_SETTING_BLOCK);
  TestHelper("testPlugin", "web_view/shim", NEEDS_TEST_SERVER);
}

#if defined(OS_WIN)
// Test is disabled on Windows because it times out often.
// http://crbug.com/403325
#define MAYBE_WebViewInBackgroundPage \
    DISABLED_WebViewInBackgroundPage
#else
#define MAYBE_WebViewInBackgroundPage WebViewInBackgroundPage
#endif
IN_PROC_BROWSER_TEST_P(WebViewTest, MAYBE_WebViewInBackgroundPage) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("platform_apps/web_view/background"))
      << message_;
}

// This test verifies that the allowtransparency attribute properly propagates.
IN_PROC_BROWSER_TEST_P(WebViewTest, AllowTransparencyAndAllowScalingPropagate) {
  LoadAppWithGuest("web_view/simple");

  ASSERT_TRUE(GetGuestWebContents());
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromWebContents(GetGuestWebContents());
  ASSERT_TRUE(guest->allow_transparency());
  ASSERT_TRUE(guest->allow_scaling());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, BasicPostMessage) {
  ASSERT_TRUE(StartEmbeddedTestServer());  // For serving guest pages.
  ASSERT_TRUE(RunPlatformAppTest("platform_apps/web_view/post_message/basic"))
      << message_;
}

// Tests that webviews do get garbage collected.
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestGarbageCollect) {
  TestHelper("testGarbageCollect", "web_view/shim", NO_TEST_SERVER);
  GetGuestViewManager()->WaitForSingleViewGarbageCollected();
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestCloseNewWindowCleanup) {
  TestHelper("testCloseNewWindowCleanup", "web_view/shim", NEEDS_TEST_SERVER);
  auto gvm = GetGuestViewManager();
  gvm->WaitForLastGuestDeleted();
  ASSERT_EQ(gvm->num_embedder_processes_destroyed(), 0);
}

// Ensure that focusing a WebView while it is already focused does not blur the
// guest content.
IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestFocusWhileFocused) {
  TestHelper("testFocusWhileFocused", "web_view/shim", NO_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewTest, NestedGuestContainerBounds) {
  // TODO(lfg): https://crbug.com/581898
  if (content::BrowserPluginGuestMode::UseCrossProcessFramesForGuests())
    return;

  TestHelper("testPDFInWebview", "web_view/shim", NO_TEST_SERVER);

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(2u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(2u, guest_web_contents_list.size());

  content::WebContents* web_view_contents = guest_web_contents_list[0];
  content::WebContents* mime_handler_view_contents = guest_web_contents_list[1];

  // Make sure we've completed loading |mime_handler_view_guest|.
  bool load_success = pdf_extension_test_util::EnsurePDFHasLoaded(
      web_view_contents);
  EXPECT_TRUE(load_success);

  gfx::Rect web_view_container_bounds = web_view_contents->GetContainerBounds();
  gfx::Rect mime_handler_view_container_bounds =
      mime_handler_view_contents->GetContainerBounds();
  EXPECT_EQ(web_view_container_bounds.origin(),
            mime_handler_view_container_bounds.origin());
}

IN_PROC_BROWSER_TEST_P(WebViewTest, Shim_TestMailtoLink) {
  TestHelper("testMailtoLink", "web_view/shim", NEEDS_TEST_SERVER);
}

// Tests that a renderer navigation from an unattached guest that results in a
// server redirect works properly.
IN_PROC_BROWSER_TEST_P(WebViewTest,
                       Shim_TestRendererNavigationRedirectWhileUnattached) {
  TestHelper("testRendererNavigationRedirectWhileUnattached",
             "web_view/shim", NEEDS_TEST_SERVER);
}

// Tests that a WebView accessible resource can actually be loaded from a
// webpage in a WebView.
IN_PROC_BROWSER_TEST_P(WebViewTest, LoadWebviewAccessibleResource) {
  TestHelper("testLoadWebviewAccessibleResource",
             "web_view/load_webview_accessible_resource", NEEDS_TEST_SERVER);
}

IN_PROC_BROWSER_TEST_P(WebViewAccessibilityTest, FocusAccessibility) {
  LoadAppWithGuest("web_view/focus_accessibility");
  content::WebContents* web_contents = GetFirstAppWindowWebContents();
  content::EnableAccessibilityForWebContents(web_contents);
  content::WebContents* guest_web_contents = GetGuestWebContents();
  content::EnableAccessibilityForWebContents(guest_web_contents);

  // Wait for focus to land on the "root web area" role, representing
  // focus on the main document itself.
  while (content::GetFocusedAccessibilityNodeInfo(web_contents).role !=
             ui::AX_ROLE_ROOT_WEB_AREA) {
    content::WaitForAccessibilityFocusChange();
  }

  // Now keep pressing the Tab key until focus lands on a button.
  while (content::GetFocusedAccessibilityNodeInfo(web_contents).role !=
             ui::AX_ROLE_BUTTON) {
    content::SimulateKeyPress(
        web_contents, ui::VKEY_TAB, false, false, false, false);
    content::WaitForAccessibilityFocusChange();
  }

  // Ensure that we hit the button inside the guest frame labeled
  // "Guest button".
  ui::AXNodeData node_data =
      content::GetFocusedAccessibilityNodeInfo(web_contents);
  EXPECT_EQ("Guest button", node_data.GetStringAttribute(ui::AX_ATTR_NAME));
}

class WebViewGuestScrollTest
    : public WebViewTestBase,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 protected:
  WebViewGuestScrollTest() {}
  ~WebViewGuestScrollTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTestBase::SetUpCommandLine(command_line);

    if (testing::get<0>(GetParam())) {
      command_line->AppendSwitch(switches::kUseCrossProcessFramesForGuests);
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewGuestScrollTest);
};

class WebViewGuestScrollTouchTest : public WebViewGuestScrollTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewGuestScrollTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kTouchEvents,
                                    switches::kTouchEventsEnabled);
  }
};

namespace {

class ScrollWaiter {
 public:
  explicit ScrollWaiter(content::RenderWidgetHostView* host_view)
      : host_view_(host_view) {}
  ~ScrollWaiter() {}

  void WaitForScrollChange(gfx::Vector2dF target_offset) {
    while (target_offset != host_view_->GetLastScrollOffset())
      base::MessageLoop::current()->RunUntilIdle();
  }

 private:
  content::RenderWidgetHostView* host_view_;
};

}  // namespace

// Tests that scrolls bubble from guest to embedder.
// Create two test instances, one where the guest body is scrollable and the
// other where the body is not scrollable: fast-path scrolling will generate
// different ack results in between these two cases.
INSTANTIATE_TEST_CASE_P(WebViewScrollBubbling,
                        WebViewGuestScrollTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

// Flaky on all platforms, see http://crbug.com/544782.
IN_PROC_BROWSER_TEST_P(WebViewGuestScrollTest,
                       DISABLED_TestGuestWheelScrollsBubble) {
  LoadAppWithGuest("web_view/scrollable_embedder_and_guest");

  if (testing::get<1>(GetParam()))
    SendMessageToGuestAndWait("set_overflow_hidden", "overflow_is_hidden");

  content::WebContents* embedder_contents = GetEmbedderWebContents();

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(1u, guest_web_contents_list.size());

  content::WebContents* guest_contents = guest_web_contents_list[0];

  gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  gfx::Rect guest_rect = guest_contents->GetContainerBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  // Send scroll gesture to embedder & verify.
  content::RenderWidgetHostView* embedder_host_view =
      embedder_contents->GetRenderWidgetHostView();
  EXPECT_EQ(gfx::Vector2dF(), embedder_host_view->GetLastScrollOffset());

  // Make sure wheel events don't get filtered.
  float scroll_magnitude = 15.f;

  {
    // Scroll the embedder from a position in the embedder that is not over
    // the guest.
    gfx::Point embedder_scroll_location(
        embedder_rect.x() + embedder_rect.width() / 2,
        (embedder_rect.y() + guest_rect.y()) / 2);

    gfx::Vector2dF expected_offset(0.f, scroll_magnitude);

    ScrollWaiter waiter(embedder_host_view);

    content::SimulateMouseEvent(embedder_contents,
                                blink::WebInputEvent::MouseMove,
                                embedder_scroll_location);
    content::SimulateMouseWheelEvent(embedder_contents,
                                     embedder_scroll_location,
                                     gfx::Vector2d(0, -scroll_magnitude));
    waiter.WaitForScrollChange(expected_offset);
  }

  content::RenderWidgetHostView* guest_host_view =
      guest_contents->GetRenderWidgetHostView();
  EXPECT_EQ(gfx::Vector2dF(), guest_host_view->GetLastScrollOffset());

  // Send scroll gesture to guest and verify embedder scrolls.
  // Perform a scroll gesture of the same magnitude, but in the opposite
  // direction and centered over the GuestView this time.
  guest_rect = guest_contents->GetContainerBounds();
  embedder_rect = embedder_contents->GetContainerBounds();
  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  {
    gfx::Point guest_scroll_location(guest_rect.x() + guest_rect.width() / 2,
                                     guest_rect.y());
    ScrollWaiter waiter(embedder_host_view);

    content::SimulateMouseEvent(embedder_contents,
                                blink::WebInputEvent::MouseMove,
                                guest_scroll_location);
    content::SimulateMouseWheelEvent(embedder_contents, guest_scroll_location,
                                     gfx::Vector2d(0, scroll_magnitude));

    waiter.WaitForScrollChange(gfx::Vector2dF());
  }
}

INSTANTIATE_TEST_CASE_P(WebViewScrollBubbling,
                        WebViewGuestScrollTouchTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

IN_PROC_BROWSER_TEST_P(WebViewGuestScrollTouchTest,
                       TestGuestGestureScrollsBubble) {
  // Just in case we're running ChromeOS tests, we need to make sure the
  // debounce interval is set to zero so our back-to-back gesture-scrolls don't
  // get munged together. Since the first scroll will be put on the fast
  // (compositor) path, while the second one should always be slow-path so it
  // gets to BrowserPlugin, having them merged is definitely an error.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_scroll_debounce_interval_in_ms(0);

  LoadAppWithGuest("web_view/scrollable_embedder_and_guest");

  if (testing::get<1>(GetParam()))
    SendMessageToGuestAndWait("set_overflow_hidden", "overflow_is_hidden");

  content::WebContents* embedder_contents = GetEmbedderWebContents();

  std::vector<content::WebContents*> guest_web_contents_list;
  GetGuestViewManager()->WaitForNumGuestsCreated(1u);
  GetGuestViewManager()->GetGuestWebContentsList(&guest_web_contents_list);
  ASSERT_EQ(1u, guest_web_contents_list.size());

  content::WebContents* guest_contents = guest_web_contents_list[0];

  gfx::Rect embedder_rect = embedder_contents->GetContainerBounds();
  gfx::Rect guest_rect = guest_contents->GetContainerBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  // Send scroll gesture to embedder & verify.
  content::RenderWidgetHostView* embedder_host_view =
      embedder_contents->GetRenderWidgetHostView();
  EXPECT_EQ(gfx::Vector2dF(), embedder_host_view->GetLastScrollOffset());

  float gesture_distance = 15.f;
  {
    // Scroll the embedder from a position in the embedder that is not over
    // the guest.
    gfx::Point embedder_scroll_location(
        embedder_rect.x() + embedder_rect.width() / 2,
        (embedder_rect.y() + guest_rect.y()) / 2);

    gfx::Vector2dF expected_offset(0.f, gesture_distance);

    content::SimulateGestureScrollSequence(
        embedder_contents, embedder_scroll_location,
        gfx::Vector2dF(0, -gesture_distance));

    ScrollWaiter waiter(embedder_host_view);

    waiter.WaitForScrollChange(expected_offset);
  }

  content::RenderWidgetHostView* guest_host_view =
      guest_contents->GetRenderWidgetHostView();
  EXPECT_EQ(gfx::Vector2dF(), guest_host_view->GetLastScrollOffset());

  // Send scroll gesture to guest and verify embedder scrolls.
  // Perform a scroll gesture of the same magnitude, but in the opposite
  // direction and centered over the GuestView this time.
  guest_rect = guest_contents->GetContainerBounds();
  embedder_rect = embedder_contents->GetContainerBounds();
  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  {
    gfx::Point guest_scroll_location(guest_rect.x() + guest_rect.width() / 2,
                                     guest_rect.y());

    ScrollWaiter waiter(embedder_host_view);

    content::SimulateGestureScrollSequence(embedder_contents,
                                           guest_scroll_location,
                                           gfx::Vector2dF(0, gesture_distance));

    waiter.WaitForScrollChange(gfx::Vector2dF());
  }
}

#if defined(USE_AURA)
// TODO(wjmaclean): when WebViewTest is re-enabled on the site-isolation
// bots, then re-enable this test class as well.
// https://crbug.com/503751
class WebViewFocusTest : public WebViewTest {
 public:
  ~WebViewFocusTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebViewTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kTouchEvents,
                                    switches::kTouchEventsEnabled);
  }

  void ForceCompositorFrame() {
    if (!frame_watcher_) {
      frame_watcher_ = new content::FrameWatcher();
      frame_watcher_->AttachTo(GetEmbedderWebContents());
    }

    while (!RequestFrame(GetEmbedderWebContents())) {
      // RequestFrame failed because we were waiting on an ack ... wait a short
      // time and retry.
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(10));
      run_loop.Run();
    }
    frame_watcher_->WaitFrames(1);
  }

 private:
  scoped_refptr<content::FrameWatcher> frame_watcher_;
};
INSTANTIATE_TEST_CASE_P(WebViewTests, WebViewFocusTest, testing::Values(false));

// The following test verifies that a views::WebView hosting an embedder
// gains focus on touchstart.
IN_PROC_BROWSER_TEST_P(WebViewFocusTest, TouchFocusesEmbedder) {
  LoadAppWithGuest("web_view/accept_touch_events");

  content::WebContents* web_contents = GetEmbedderWebContents();
  content::RenderViewHost* embedder_rvh = web_contents->GetRenderViewHost();

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

  extensions::AppWindow* app_window = GetFirstAppWindowForBrowser(browser());
  aura::Window* window = app_window->GetNativeWindow();
  EXPECT_TRUE(app_window);
  EXPECT_TRUE(window);
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window);
  EXPECT_TRUE(widget->GetRootView());
  // We only expect a single views::webview in the view hierarchy.
  views::View* aura_webview = FindWebView(widget->GetRootView());
  ASSERT_TRUE(aura_webview);
  gfx::Rect bounds(aura_webview->bounds());
  EXPECT_TRUE(aura_webview->IsFocusable());

  views::View* other_focusable_view = new views::View();
  other_focusable_view->SetBounds(bounds.x() + bounds.width(), bounds.y(), 100,
                                  100);
  other_focusable_view->SetFocusable(true);
  aura_webview->parent()->AddChildView(other_focusable_view);
  other_focusable_view->SetPosition(gfx::Point(bounds.x() + bounds.width(), 0));

  // Sync changes to compositor.
  ForceCompositorFrame();

  aura_webview->RequestFocus();
  // Verify that other_focusable_view can steal focus from aura_webview.
  EXPECT_TRUE(aura_webview->HasFocus());
  other_focusable_view->RequestFocus();
  EXPECT_TRUE(other_focusable_view->HasFocus());
  EXPECT_FALSE(aura_webview->HasFocus());

  // Compute location of guest within embedder so we can more accurately
  // target our touch event.
  gfx::Rect guest_rect = GetGuestWebContents()->GetContainerBounds();
  gfx::Point embedder_origin =
      GetEmbedderWebContents()->GetContainerBounds().origin();
  guest_rect.Offset(-embedder_origin.x(), -embedder_origin.y());

  // Generate and send synthetic touch event.
  content::SimulateTouchPressAt(GetEmbedderWebContents(),
                                guest_rect.CenterPoint());
  EXPECT_TRUE(aura_webview->HasFocus());
}
#endif

#if defined(ENABLE_TASK_MANAGER)

namespace {

base::string16 GetExpectedPrefix(content::WebContents* web_contents) {
  DCHECK(web_contents);
  guest_view::GuestViewBase* guest =
      guest_view::GuestViewBase::FromWebContents(web_contents);
  DCHECK(guest);

  return l10n_util::GetStringFUTF16(guest->GetTaskPrefix(), base::string16());
}

const std::vector<task_management::WebContentsTag*>& GetTrackedTags() {
  return task_management::WebContentsTagsManager::GetInstance()->
      tracked_tags();
}

bool HasExpectedGuestTask(
    const task_management::MockWebContentsTaskManager& task_manager,
    content::WebContents* guest_contents) {
  bool found = false;
  for (auto* task: task_manager.tasks()) {
    if (task->GetType() != task_management::Task::GUEST)
      continue;
    EXPECT_FALSE(found);
    found = true;
    const base::string16 title = task->title();
    const base::string16 expected_prefix = GetExpectedPrefix(guest_contents);
    EXPECT_TRUE(base::StartsWith(title, expected_prefix,
                                 base::CompareCase::INSENSITIVE_ASCII));
  }
  return found;
}

}  // namespace

// Tests that the pre-existing WebViews are provided to the task manager.
IN_PROC_BROWSER_TEST_P(WebViewTest, TaskManagementPreExistingWebViews) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Browser tests start with a single tab.
  EXPECT_EQ(1U, GetTrackedTags().size());

  content::WebContents* guest_contents =
      LoadGuest("/extensions/platform_apps/web_view/task_manager/guest.html",
                "web_view/task_manager");

  task_management::MockWebContentsTaskManager task_manager;
  task_manager.StartObserving();

  // The pre-existing tab and guest tasks are provided.
  // 4 tasks expected. The order is arbitrary.
  // Tab: about:blank,
  // Background Page: <webview> task manager test,
  // App: <webview> task manager test,
  // Webview: WebViewed test content.
  EXPECT_EQ(4U, task_manager.tasks().size());
  EXPECT_TRUE(HasExpectedGuestTask(task_manager, guest_contents));
}

// Tests that the post-existing WebViews are provided to the task manager.
IN_PROC_BROWSER_TEST_P(WebViewTest, TaskManagementPostExistingWebViews) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Browser tests start with a single tab.
  EXPECT_EQ(1U, GetTrackedTags().size());

  task_management::MockWebContentsTaskManager task_manager;
  task_manager.StartObserving();

  // Only the "about:blank" tab shows at the moment.
  ASSERT_EQ(1U, task_manager.tasks().size());
  const task_management::Task* about_blank_task = task_manager.tasks().back();
  EXPECT_EQ(task_management::Task::RENDERER, about_blank_task->GetType());
  EXPECT_EQ(base::UTF8ToUTF16("Tab: about:blank"), about_blank_task->title());

  // Now load a guest web view.
  content::WebContents* guest_contents =
      LoadGuest("/extensions/platform_apps/web_view/task_manager/guest.html",
                "web_view/task_manager");
  // 4 tasks expected. The order is arbitrary.
  // Tab: about:blank,
  // Background Page: <webview> task manager test,
  // App: <webview> task manager test,
  // Webview: WebViewed test content.
  EXPECT_EQ(4U, task_manager.tasks().size());
  EXPECT_TRUE(HasExpectedGuestTask(task_manager, guest_contents));
}

#endif  // defined(ENABLE_TASK_MANAGER)
