// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <set>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_browsertest.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/web/web_context_menu_data.h"

using content::ResourceType;
using content::WebContents;

namespace extensions {

namespace {

// Waits for a WC to be created. Once it starts loading |delay_url| (after at
// least the first navigation has committed), it delays the load, executes
// |script| in the last committed RVH and resumes the load when a URL ending in
// |until_url_suffix| commits. This class expects |script| to trigger the load
// of an URL ending in |until_url_suffix|.
class DelayLoadStartAndExecuteJavascript
    : public content::NotificationObserver,
      public content::WebContentsObserver {
 public:
  DelayLoadStartAndExecuteJavascript(
      const GURL& delay_url,
      const std::string& script,
      const std::string& until_url_suffix)
      : content::WebContentsObserver(),
        delay_url_(delay_url),
        until_url_suffix_(until_url_suffix),
        script_(script),
        has_user_gesture_(false),
        script_was_executed_(false),
        rfh_(nullptr) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_ADDED,
                   content::NotificationService::AllSources());
  }
  ~DelayLoadStartAndExecuteJavascript() override {}

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    if (type != chrome::NOTIFICATION_TAB_ADDED) {
      NOTREACHED();
      return;
    }
    content::WebContentsObserver::Observe(
        content::Details<content::WebContents>(details).ptr());
    registrar_.RemoveAll();
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->GetURL() != delay_url_ || !rfh_)
      return;

    auto throttle =
        std::make_unique<WillStartRequestObserverThrottle>(navigation_handle);
    throttle_ = throttle->AsWeakPtr();
    navigation_handle->RegisterThrottleForTesting(std::move(throttle));

    if (has_user_gesture_) {
      rfh_->ExecuteJavaScriptWithUserGestureForTests(
          base::UTF8ToUTF16(script_));
    } else {
      rfh_->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script_));
    }
    script_was_executed_ = true;
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage())
      return;

    if (script_was_executed_ &&
        base::EndsWith(navigation_handle->GetURL().spec(), until_url_suffix_,
                       base::CompareCase::SENSITIVE)) {
      content::WebContentsObserver::Observe(NULL);
      if (throttle_)
        throttle_->Unblock();
    }

    if (navigation_handle->IsInMainFrame())
      rfh_ = navigation_handle->GetRenderFrameHost();
  }

  void set_has_user_gesture(bool has_user_gesture) {
    has_user_gesture_ = has_user_gesture;
  }

 private:
  class WillStartRequestObserverThrottle
      : public content::NavigationThrottle,
        public base::SupportsWeakPtr<WillStartRequestObserverThrottle> {
   public:
    explicit WillStartRequestObserverThrottle(content::NavigationHandle* handle)
        : NavigationThrottle(handle) {}
    ~WillStartRequestObserverThrottle() override {}

    const char* GetNameForLogging() override {
      return "WillStartRequestObserverThrottle";
    }

    void Unblock() {
      DCHECK(throttled_);
      Resume();
    }

   private:
    NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
      throttled_ = true;
      return NavigationThrottle::DEFER;
    }

    bool throttled_ = false;
  };

  content::NotificationRegistrar registrar_;

  base::WeakPtr<WillStartRequestObserverThrottle> throttle_;

  GURL delay_url_;
  std::string until_url_suffix_;
  std::string script_;
  bool has_user_gesture_;
  bool script_was_executed_;
  content::RenderFrameHost* rfh_;

  DISALLOW_COPY_AND_ASSIGN(DelayLoadStartAndExecuteJavascript);
};

// Handles requests for URLs with paths of "/test*" sent to the test server, so
// tests request a URL that receives a non-error response.
std::unique_ptr<net::test_server::HttpResponse> HandleTestRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, "/test",
                        base::CompareCase::SENSITIVE)) {
    return nullptr;
  }
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content("This space intentionally left blank.");
  return std::move(response);
}

}  // namespace

class WebNavigationApiTest : public ExtensionApiTest {
 public:
  WebNavigationApiTest() {
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&HandleTestRequest));
  }
  ~WebNavigationApiTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    FrameNavigationState::set_allow_extension_scheme(true);

    host_resolver()->AddRule("*", "127.0.0.1");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNavigationApiTest);
};

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, Api) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, GetFrame) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/getFrame")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ClientRedirect) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/clientRedirect"))
      << message_;
}

// http://crbug.com/660288
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, DISABLED_ServerRedirect) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/serverRedirect"))
      << message_;
}

// https://crbug.com/660288
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, DISABLED_Download) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser()->profile());
  content::DownloadTestObserverTerminal observer(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  bool result = RunExtensionTest("webnavigation/download");
  observer.WaitForFinished();
  ASSERT_TRUE(result) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ServerRedirectSingleProcess) {
  // TODO(lukasza): https://crbug.com/671734: Investigate why this test fails
  // with --site-per-process.
  if (content::AreAllSitesIsolatedForTesting())
    return;

  ASSERT_TRUE(StartEmbeddedTestServer());

  // Set max renderers to 1 to force running out of processes.
  content::RenderProcessHost::SetMaxRendererProcessCount(1);

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(
      RunExtensionTest("webnavigation/serverRedirectSingleProcess"))
      << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;
  GURL url(
      base::StringPrintf("http://www.a.com:%u/extensions/api_test/"
                         "webnavigation/serverRedirectSingleProcess/a.html",
                         embedded_test_server()->port()));

  ui_test_utils::NavigateToURL(browser(), url);

  url = GURL(base::StringPrintf(
      "http://www.b.com:%u/server-redirect?http://www.b.com:%u/test",
      embedded_test_server()->port(), embedded_test_server()->port()));

  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ForwardBack) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/forwardBack")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, IFrame) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, SrcDoc) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/srcdoc")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, OpenTab) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/openTab")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ReferenceFragment) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/referenceFragment"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, SimpleLoad) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/simpleLoad")) << message_;
}

// Flaky on Windows, Mac and Linux. See http://crbug.com/477480 (Windows) and
// https://crbug.com/746407 (Mac, Linux).
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_Failures DISABLED_Failures
#else
#define MAYBE_Failures Failures
#endif
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, MAYBE_Failures) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/failures")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, FilteredTest) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/filtered")) << message_;
}

// Flaky on Windows. See http://crbug.com/662160.
#if defined(OS_WIN)
#define MAYBE_UserAction DISABLED_UserAction
#else
#define MAYBE_UserAction UserAction
#endif
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, MAYBE_UserAction) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/userAction")) << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);
  GURL url = extension->GetResourceURL(
      "a.html?" + base::IntToString(embedded_test_server()->port()));

  ui_test_utils::NavigateToURL(browser(), url);

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::WebContextMenuData::kMediaTypeNone;
  params.page_url = url;
  params.link_url = extension->GetResourceURL("b.html");

  // Get the child frame, which will be the one associated with the context
  // menu.
  std::vector<content::RenderFrameHost*> frames = tab->GetAllFrames();
  EXPECT_EQ(2UL, frames.size());
  EXPECT_TRUE(frames[1]->GetParent());

  TestRenderViewContextMenu menu(frames[1], params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, RequestOpenTab) {

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/requestOpenTab"))
      << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;

  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();
  const extensions::Extension* extension =
      service->GetExtensionById(last_loaded_extension_id(), false);
  GURL url = extension->GetResourceURL("a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // There's a link on a.html. Middle-click on it to open it in a new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kMiddle;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, TargetBlank) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/targetBlank")) << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;

  GURL url = embedded_test_server()->GetURL(
      "/extensions/api_test/webnavigation/targetBlank/a.html");

  NavigateParams params(browser(), url, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, TargetBlankIncognito) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTestIncognito("webnavigation/targetBlank"))
      << message_;

  ResultCatcher catcher;

  GURL url = embedded_test_server()->GetURL(
      "/extensions/api_test/webnavigation/targetBlank/a.html");

  Browser* otr_browser = OpenURLOffTheRecord(browser()->profile(), url);
  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(7, 7);
  mouse_event.click_count = 1;
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  tab->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, History) {
  ASSERT_TRUE(RunExtensionTest("webnavigation/history")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, CrossProcess) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadExtension(test_data_dir_.AppendASCII("webnavigation").AppendASCII("app"));

  // See crossProcess/d.html.
  DelayLoadStartAndExecuteJavascript call_script(
      embedded_test_server()->GetURL("/test1"),
      "navigate2()",
      "empty.html");

  DelayLoadStartAndExecuteJavascript call_script_user_gesture(
      embedded_test_server()->GetURL("/test2"),
      "navigate2()",
      "empty.html");
  call_script_user_gesture.set_has_user_gesture(true);

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcess")) << message_;
}

// crbug.com/708139.
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, DISABLED_CrossProcessFragment) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // See crossProcessFragment/f.html.
  DelayLoadStartAndExecuteJavascript call_script3(
      embedded_test_server()->GetURL("/test3"),
      "updateFragment()",
      base::StringPrintf("f.html?%u#foo", embedded_test_server()->port()));

  // See crossProcessFragment/g.html.
  DelayLoadStartAndExecuteJavascript call_script4(
      embedded_test_server()->GetURL("/test4"),
      "updateFragment()",
      base::StringPrintf("g.html?%u#foo", embedded_test_server()->port()));

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessFragment"))
      << message_;
}

// crbug.com/708139.
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, DISABLED_CrossProcessHistory) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // See crossProcessHistory/e.html.
  DelayLoadStartAndExecuteJavascript call_script2(
      embedded_test_server()->GetURL("/test2"),
      "updateHistory()",
      "empty.html");

  // See crossProcessHistory/h.html.
  DelayLoadStartAndExecuteJavascript call_script5(
      embedded_test_server()->GetURL("/test5"),
      "updateHistory()",
      "empty.html");

  // See crossProcessHistory/i.html.
  DelayLoadStartAndExecuteJavascript call_script6(
      embedded_test_server()->GetURL("/test6"),
      "updateHistory()",
      "empty.html");

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessHistory"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, CrossProcessIframe) {
  content::IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessIframe")) << message_;
}

// TODO(jam): http://crbug.com/350550
#if !(defined(OS_CHROMEOS) && defined(ADDRESS_SANITIZER))
IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, Crash) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Wait for the extension to set itself up and return control to us.
  ASSERT_TRUE(RunExtensionTest("webnavigation/crash")) << message_;

  WebContents* tab = browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(tab);

  ResultCatcher catcher;

  GURL url(base::StringPrintf(
      "http://www.a.com:%u/"
          "extensions/api_test/webnavigation/crash/a.html",
      embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));

  url = GURL(base::StringPrintf(
      "http://www.a.com:%u/"
          "extensions/api_test/webnavigation/crash/b.html",
      embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#endif

}  // namespace extensions
