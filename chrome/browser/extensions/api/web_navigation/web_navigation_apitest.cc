// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <set>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/chrome_browser_main_extra_parts.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/download/download_browsertest.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/extensions/api/web_navigation/web_navigation_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/renderer_host/chrome_resource_dispatcher_host_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_controller.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/switches.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/WebKit/public/web/WebContextMenuData.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using content::ResourceType;
using content::WebContents;

namespace extensions {

namespace {

// This class can defer requests for arbitrary URLs.
class TestNavigationListener
    : public base::RefCountedThreadSafe<TestNavigationListener> {
 public:
  TestNavigationListener() {}

  // Add |url| to the set of URLs we should delay.
  void DelayRequestsForURL(const GURL& url) {
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&TestNavigationListener::DelayRequestsForURL, this, url));
      return;
    }
    urls_to_delay_.insert(url);
  }

  // Resume all deferred requests.
  void ResumeAll() {
    if (!content::BrowserThread::CurrentlyOn(content::BrowserThread::IO)) {
      content::BrowserThread::PostTask(
          content::BrowserThread::IO,
          FROM_HERE,
          base::Bind(&TestNavigationListener::ResumeAll, this));
      return;
    }
    WeakThrottleList::const_iterator it;
    for (it = throttles_.begin(); it != throttles_.end(); ++it) {
      if (it->get())
        (*it)->Resume();
    }
    throttles_.clear();
  }

  // Constructs a ResourceThrottle if the request for |url| should be held.
  //
  // Needs to be invoked on the IO thread.
  content::ResourceThrottle* CreateResourceThrottle(
      const GURL& url,
      ResourceType resource_type) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    if (urls_to_delay_.find(url) == urls_to_delay_.end())
      return NULL;

    Throttle* throttle = new Throttle();
    throttles_.push_back(throttle->AsWeakPtr());
    return throttle;
  }

 private:
  friend class base::RefCountedThreadSafe<TestNavigationListener>;

  virtual ~TestNavigationListener() {}

  // Stores a throttle per URL request that we have delayed.
  class Throttle : public content::ResourceThrottle,
                   public base::SupportsWeakPtr<Throttle> {
   public:
    void Resume() {
      controller()->Resume();
    }

    // content::ResourceThrottle implementation.
    virtual void WillStartRequest(bool* defer) OVERRIDE {
      *defer = true;
    }

    virtual const char* GetNameForLogging() const OVERRIDE {
      return "TestNavigationListener::Throttle";
    }
  };
  typedef base::WeakPtr<Throttle> WeakThrottle;
  typedef std::list<WeakThrottle> WeakThrottleList;
  WeakThrottleList throttles_;

  // The set of URLs to be delayed.
  std::set<GURL> urls_to_delay_;

  DISALLOW_COPY_AND_ASSIGN(TestNavigationListener);
};

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
      TestNavigationListener* test_navigation_listener,
      const GURL& delay_url,
      const std::string& script,
      const std::string& until_url_suffix)
      : content::WebContentsObserver(),
        test_navigation_listener_(test_navigation_listener),
        delay_url_(delay_url),
        until_url_suffix_(until_url_suffix),
        script_(script),
        script_was_executed_(false),
        rvh_(NULL) {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TAB_ADDED,
                   content::NotificationService::AllSources());
    test_navigation_listener_->DelayRequestsForURL(delay_url_);
  }
  virtual ~DelayLoadStartAndExecuteJavascript() {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type != chrome::NOTIFICATION_TAB_ADDED) {
      NOTREACHED();
      return;
    }
    content::WebContentsObserver::Observe(
        content::Details<content::WebContents>(details).ptr());
    registrar_.RemoveAll();
  }

  virtual void DidStartProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& validated_url,
      bool is_error_page,
      bool is_iframe_srcdoc) OVERRIDE {
    if (validated_url != delay_url_ || !rvh_)
      return;

    rvh_->GetMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(script_));
    script_was_executed_ = true;
  }

  virtual void DidCommitProvisionalLoadForFrame(
      content::RenderFrameHost* render_frame_host,
      const GURL& url,
      content::PageTransition transition_type) OVERRIDE {
    if (script_was_executed_ && EndsWith(url.spec(), until_url_suffix_, true)) {
      content::WebContentsObserver::Observe(NULL);
      test_navigation_listener_->ResumeAll();
    }
    rvh_ = render_frame_host->GetRenderViewHost();
  }

 private:
  content::NotificationRegistrar registrar_;

  scoped_refptr<TestNavigationListener> test_navigation_listener_;

  GURL delay_url_;
  std::string until_url_suffix_;
  std::string script_;
  bool script_was_executed_;
  content::RenderViewHost* rvh_;

  DISALLOW_COPY_AND_ASSIGN(DelayLoadStartAndExecuteJavascript);
};

// A ResourceDispatcherHostDelegate that adds a TestNavigationObserver.
class TestResourceDispatcherHostDelegate
    : public ChromeResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate(
      prerender::PrerenderTracker* prerender_tracker,
      TestNavigationListener* test_navigation_listener)
      : ChromeResourceDispatcherHostDelegate(prerender_tracker),
        test_navigation_listener_(test_navigation_listener) {
  }
  virtual ~TestResourceDispatcherHostDelegate() {}

  virtual void RequestBeginning(
      net::URLRequest* request,
      content::ResourceContext* resource_context,
      content::AppCacheService* appcache_service,
      ResourceType resource_type,
      int child_id,
      int route_id,
      ScopedVector<content::ResourceThrottle>* throttles) OVERRIDE {
    ChromeResourceDispatcherHostDelegate::RequestBeginning(
        request,
        resource_context,
        appcache_service,
        resource_type,
        child_id,
        route_id,
        throttles);
    content::ResourceThrottle* throttle =
        test_navigation_listener_->CreateResourceThrottle(request->url(),
                                                          resource_type);
    if (throttle)
      throttles->push_back(throttle);
  }

 private:
  scoped_refptr<TestNavigationListener> test_navigation_listener_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

}  // namespace

class WebNavigationApiTest : public ExtensionApiTest {
 public:
  WebNavigationApiTest() {}
  virtual ~WebNavigationApiTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();

    FrameNavigationState::set_allow_extension_scheme(true);

    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAllowLegacyExtensionManifests);

    host_resolver()->AddRule("*", "127.0.0.1");
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    ExtensionApiTest::SetUpOnMainThread();
    test_navigation_listener_ = new TestNavigationListener();
    resource_dispatcher_host_delegate_.reset(
        new TestResourceDispatcherHostDelegate(
            g_browser_process->prerender_tracker(),
            test_navigation_listener_.get()));
    content::ResourceDispatcherHost::Get()->SetDelegate(
        resource_dispatcher_host_delegate_.get());
  }

  TestNavigationListener* test_navigation_listener() {
    return test_navigation_listener_.get();
  }

 private:
  scoped_refptr<TestNavigationListener> test_navigation_listener_;
  scoped_ptr<TestResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  DISALLOW_COPY_AND_ASSIGN(WebNavigationApiTest);
};

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, Api) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, GetFrame) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/getFrame")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ClientRedirect) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/clientRedirect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ServerRedirect) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/serverRedirect"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, Download) {
  base::ScopedTempDir download_directory;
  ASSERT_TRUE(download_directory.CreateUniqueTempDir());
  DownloadPrefs* download_prefs =
      DownloadPrefs::FromBrowserContext(browser()->profile());
  download_prefs->SetDownloadPath(download_directory.path());

  DownloadTestObserverNotInProgress download_observer(
      content::BrowserContext::GetDownloadManager(profile()), 1);
  download_observer.StartObserving();
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/download"))
      << message_;
  download_observer.WaitForFinished();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ServerRedirectSingleProcess) {
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
  GURL url(base::StringPrintf(
      "http://www.a.com:%d/"
      "extensions/api_test/webnavigation/serverRedirectSingleProcess/a.html",
      embedded_test_server()->port()));

  ui_test_utils::NavigateToURL(browser(), url);

  url = GURL(base::StringPrintf(
      "http://www.b.com:%d/server-redirect?http://www.b.com:%d/",
      embedded_test_server()->port(),
      embedded_test_server()->port()));

  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ForwardBack) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/forwardBack")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, IFrame) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/iframe")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, SrcDoc) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/srcdoc")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, OpenTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/openTab")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, ReferenceFragment) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/referenceFragment"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, SimpleLoad) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/simpleLoad")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, Failures) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/failures")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, FilteredTest) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/filtered")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, UserAction) {
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
  GURL url = extension->GetResourceURL("a.html");

  ui_test_utils::NavigateToURL(browser(), url);

  // This corresponds to "Open link in new tab".
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::WebContextMenuData::MediaTypeNone;
  params.page_url = url;
  params.link_url = extension->GetResourceURL("b.html");

  TestRenderViewContextMenu menu(tab->GetMainFrame(), params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, RequestOpenTab) {
  ASSERT_TRUE(StartEmbeddedTestServer());

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
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = blink::WebMouseEvent::ButtonMiddle;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

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

  chrome::NavigateParams params(browser(), url, content::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = blink::WebMouseEvent::ButtonLeft;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

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

  Browser* otr_browser = ui_test_utils::OpenURLOffTheRecord(
      browser()->profile(), url);
  WebContents* tab = otr_browser->tab_strip_model()->GetActiveWebContents();

  // There's a link with target=_blank on a.html. Click on it to open it in a
  // new tab.
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = blink::WebMouseEvent::ButtonLeft;
  mouse_event.x = 7;
  mouse_event.y = 7;
  mouse_event.clickCount = 1;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  tab->GetRenderViewHost()->ForwardMouseEvent(mouse_event);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, History) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("webnavigation/history")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, CrossProcess) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  LoadExtension(test_data_dir_.AppendASCII("webnavigation").AppendASCII("app"));

  // See crossProcess/d.html.
  DelayLoadStartAndExecuteJavascript call_script(
      test_navigation_listener(),
      embedded_test_server()->GetURL("/test1"),
      "navigate2()",
      "empty.html");

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcess")) << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, CrossProcessFragment) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // See crossProcessFragment/f.html.
  DelayLoadStartAndExecuteJavascript call_script3(
      test_navigation_listener(),
      embedded_test_server()->GetURL("/test3"),
      "updateFragment()",
      base::StringPrintf("f.html?%d#foo", embedded_test_server()->port()));

  // See crossProcessFragment/g.html.
  DelayLoadStartAndExecuteJavascript call_script4(
      test_navigation_listener(),
      embedded_test_server()->GetURL("/test4"),
      "updateFragment()",
      base::StringPrintf("g.html?%d#foo", embedded_test_server()->port()));

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessFragment"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(WebNavigationApiTest, CrossProcessHistory) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  // See crossProcessHistory/e.html.
  DelayLoadStartAndExecuteJavascript call_script2(
      test_navigation_listener(),
      embedded_test_server()->GetURL("/test2"),
      "updateHistory()",
      "empty.html");

  // See crossProcessHistory/h.html.
  DelayLoadStartAndExecuteJavascript call_script5(
      test_navigation_listener(),
      embedded_test_server()->GetURL("/test5"),
      "updateHistory()",
      "empty.html");

  // See crossProcessHistory/i.html.
  DelayLoadStartAndExecuteJavascript call_script6(
      test_navigation_listener(),
      embedded_test_server()->GetURL("/test6"),
      "updateHistory()",
      "empty.html");

  ASSERT_TRUE(RunExtensionTest("webnavigation/crossProcessHistory"))
      << message_;
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
      "http://www.a.com:%d/"
          "extensions/api_test/webnavigation/crash/a.html",
      embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);

  ui_test_utils::NavigateToURL(browser(), GURL(content::kChromeUICrashURL));

  url = GURL(base::StringPrintf(
      "http://www.a.com:%d/"
          "extensions/api_test/webnavigation/crash/b.html",
      embedded_test_server()->port()));
  ui_test_utils::NavigateToURL(browser(), url);

  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#endif

}  // namespace extensions
