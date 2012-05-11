// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_link_manager_factory.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/devtools_agent_host_registry.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/mock_host_resolver.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using content::DevToolsAgentHost;
using content::DevToolsAgentHostRegistry;
using content::DevToolsClientHost;
using content::DevToolsManager;
using content::NavigationController;
using content::OpenURLParams;
using content::Referrer;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::WebContents;

// Prerender tests work as follows:
//
// A page with a prefetch link to the test page is loaded.  Once prerendered,
// its Javascript function DidPrerenderPass() is called, which returns true if
// the page behaves as expected when prerendered.
//
// The prerendered page is then displayed on a tab.  The Javascript function
// DidDisplayPass() is called, and returns true if the page behaved as it
// should while being displayed.

namespace prerender {

namespace {

// Constants used in the test HTML files.
static const char* kReadyTitle = "READY";
static const char* kPassTitle = "PASS";

std::string CreateClientRedirect(const std::string& dest_url) {
  const char* const kClientRedirectBase = "client-redirect?";
  return kClientRedirectBase + dest_url;
}

std::string CreateServerRedirect(const std::string& dest_url) {
  const char* const kServerRedirectBase = "server-redirect?";
  return kServerRedirectBase + dest_url;
}

// Clears the specified data using BrowsingDataRemover.
void ClearBrowsingData(Browser* browser, int remove_mask) {
  BrowsingDataRemover* remover =
      new BrowsingDataRemover(browser->profile(),
                              BrowsingDataRemover::EVERYTHING,
                              base::Time());
  remover->Remove(remove_mask);
  // BrowsingDataRemover deletes itself.
}

void CancelAllPrerenders(PrerenderManager* prerender_manager) {
  prerender_manager->CancelAllPrerenders();
}

// Returns true if and only if the final status is one in which the prerendered
// page should prerender correctly. The page still may not be used.
bool ShouldRenderPrerenderedPageCorrectly(FinalStatus status) {
  switch (status) {
    case FINAL_STATUS_USED:
    case FINAL_STATUS_WINDOW_OPENER:
    case FINAL_STATUS_APP_TERMINATING:
    case FINAL_STATUS_FRAGMENT_MISMATCH:
    case FINAL_STATUS_CACHE_OR_HISTORY_CLEARED:
    // We'll crash the renderer after it's loaded.
    case FINAL_STATUS_RENDERER_CRASHED:
    case FINAL_STATUS_CANCELLED:
    case FINAL_STATUS_DEVTOOLS_ATTACHED:
    case FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH:
      return true;
    default:
      return false;
  }
}

// PrerenderContents that stops the UI message loop on DidStopLoading().
class TestPrerenderContents : public PrerenderContents {
 public:
  TestPrerenderContents(
      PrerenderManager* prerender_manager,
      PrerenderTracker* prerender_tracker,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      int expected_number_of_loads,
      FinalStatus expected_final_status,
      bool prerender_should_wait_for_ready_title)
      : PrerenderContents(prerender_manager, prerender_tracker,
                          profile, url, referrer, ORIGIN_LINK_REL_PRERENDER,
                          PrerenderManager::kNoExperiment),
        number_of_loads_(0),
        expected_number_of_loads_(expected_number_of_loads),
        expected_final_status_(expected_final_status),
        new_render_view_host_(NULL),
        was_hidden_(false),
        was_shown_(false),
        should_be_shown_(expected_final_status == FINAL_STATUS_USED),
        quit_message_loop_on_destruction_(
            expected_final_status != FINAL_STATUS_EVICTED &&
            expected_final_status != FINAL_STATUS_APP_TERMINATING &&
            expected_final_status != FINAL_STATUS_MAX),
        expected_pending_prerenders_(0),
        prerender_should_wait_for_ready_title_(
            prerender_should_wait_for_ready_title) {
    if (expected_number_of_loads == 0)
      MessageLoopForUI::current()->Quit();
  }

  virtual ~TestPrerenderContents() {
    if (expected_final_status_ == FINAL_STATUS_MAX) {
      EXPECT_EQ(match_complete_status(), MATCH_COMPLETE_REPLACEMENT);
    } else {
      EXPECT_EQ(expected_final_status_, final_status()) <<
          " when testing URL " << prerender_url().path() <<
          " (Expected: " << NameFromFinalStatus(expected_final_status_) <<
          ", Actual: " << NameFromFinalStatus(final_status()) << ")";
    }
    // Prerendering RenderViewHosts should be hidden before the first
    // navigation, so this should be happen for every PrerenderContents for
    // which a RenderViewHost is created, regardless of whether or not it's
    // used.
    if (new_render_view_host_)
      EXPECT_TRUE(was_hidden_);

    // A used PrerenderContents will only be destroyed when we swap out
    // WebContents, at the end of a navigation caused by a call to
    // NavigateToURLImpl().
    if (final_status() == FINAL_STATUS_USED)
      EXPECT_TRUE(new_render_view_host_);

    EXPECT_EQ(should_be_shown_, was_shown_);

    // When the PrerenderContents is destroyed, quit the UI message loop.
    // This happens on navigation to used prerendered pages, and soon
    // after cancellation of unused prerendered pages.
    if (quit_message_loop_on_destruction_) {
      // The message loop may not be running if this is swapped in
      // synchronously on a Navigation.
      MessageLoop* loop = MessageLoopForUI::current();
      if (loop->is_running())
        loop->Quit();
    }
  }

  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE {
    // On quit, it's possible to end up here when render processes are closed
    // before the PrerenderManager is destroyed.  As a result, it's possible to
    // get either FINAL_STATUS_APP_TERMINATING or FINAL_STATUS_RENDERER_CRASHED
    // on quit.
    //
    // It's also possible for this to be called after we've been notified of
    // app termination, but before we've been deleted, which is why the second
    // check is needed.
    if (expected_final_status_ == FINAL_STATUS_APP_TERMINATING &&
        final_status() != expected_final_status_) {
      expected_final_status_ = FINAL_STATUS_RENDERER_CRASHED;
    }

    PrerenderContents::RenderViewGone(status);
  }

  virtual bool AddAliasURL(const GURL& url) OVERRIDE {
    // Prevent FINAL_STATUS_UNSUPPORTED_SCHEME when navigating to about:crash in
    // the PrerenderRendererCrash test.
    if (url.spec() != chrome::kChromeUICrashURL)
      return PrerenderContents::AddAliasURL(url);
    return true;
  }

  virtual void DidStopLoading() OVERRIDE {
    PrerenderContents::DidStopLoading();
    ++number_of_loads_;
    if (ShouldRenderPrerenderedPageCorrectly(expected_final_status_) &&
        number_of_loads_ == expected_number_of_loads_) {
      MessageLoopForUI::current()->Quit();
    }
  }

  virtual void AddPendingPrerender(const GURL& url,
                                   const content::Referrer& referrer,
                                   const gfx::Size& size) OVERRIDE {
    PrerenderContents::AddPendingPrerender(url, referrer, size);
    if (expected_pending_prerenders_ > 0 &&
        pending_prerender_list()->size() == expected_pending_prerenders_) {
      MessageLoop::current()->Quit();
    }
  }

  virtual WebContents* CreateWebContents(
      content::SessionStorageNamespace* session_storage_namespace) OVERRIDE {
    WebContents* web_contents = PrerenderContents::CreateWebContents(
        session_storage_namespace);
    string16 ready_title = ASCIIToUTF16(kReadyTitle);
    if (prerender_should_wait_for_ready_title_)
      ready_title_watcher_.reset(new ui_test_utils::TitleWatcher(
          web_contents, ready_title));
    return web_contents;
  }

  void WaitForPrerenderToHaveReadyTitleIfRequired() {
    if (ready_title_watcher_.get()) {
      string16 ready_title = ASCIIToUTF16(kReadyTitle);
      ASSERT_EQ(ready_title, ready_title_watcher_->WaitAndGetTitle());
    }
  }

  // Waits until the prerender has |expected_pending_prerenders| pending
  // prerenders.
  void WaitForPendingPrerenders(size_t expected_pending_prerenders) {
    if (pending_prerender_list()->size() < expected_pending_prerenders) {
      expected_pending_prerenders_ = expected_pending_prerenders;
      ui_test_utils::RunMessageLoop();
      expected_pending_prerenders_ = 0;
    }

    EXPECT_EQ(expected_pending_prerenders, pending_prerender_list()->size());
  }

  // For tests that open the prerender in a new background tab, the RenderView
  // will not have been made visible when the PrerenderContents is destroyed
  // even though it is used.
  void set_should_be_shown(bool value) { should_be_shown_ = value; }

  int number_of_loads() const { return number_of_loads_; }

 private:
  virtual void OnRenderViewHostCreated(
      RenderViewHost* new_render_view_host) OVERRIDE {
    // Used to make sure the RenderViewHost is hidden and, if used,
    // subsequently shown.
    notification_registrar().Add(
        this,
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
        content::Source<RenderWidgetHost>(new_render_view_host));

    new_render_view_host_ = new_render_view_host;

    PrerenderContents::OnRenderViewHostCreated(new_render_view_host);
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type ==
        content::NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED) {
      EXPECT_EQ(new_render_view_host_,
                content::Source<RenderWidgetHost>(source).ptr());
      bool is_visible = *content::Details<bool>(details).ptr();

      if (!is_visible) {
        was_hidden_ = true;
      } else if (is_visible && was_hidden_) {
        // Once hidden, a prerendered RenderViewHost should only be shown after
        // being removed from the PrerenderContents for display.
        EXPECT_FALSE(GetRenderViewHost());
        was_shown_ = true;
      }
      return;
    }
    PrerenderContents::Observe(type, source, details);
  }

  int number_of_loads_;
  int expected_number_of_loads_;
  FinalStatus expected_final_status_;

  // The RenderViewHost created for the prerender, if any.
  RenderViewHost* new_render_view_host_;
  // Set to true when the prerendering RenderWidget is hidden.
  bool was_hidden_;
  // Set to true when the prerendering RenderWidget is shown, after having been
  // hidden.
  bool was_shown_;
  // Expected final value of was_shown_.  Defaults to true for
  // FINAL_STATUS_USED, and false otherwise.
  bool should_be_shown_;

  // If true, quits message loop on destruction of |this|.
  bool quit_message_loop_on_destruction_;

  // Total number of pending prerenders we're currently waiting for.  Zero
  // indicates we currently aren't waiting for any.
  size_t expected_pending_prerenders_;

  // If true, before calling DidPrerenderPass, will wait for the title of the
  // prerendered page to turn to "READY".
  bool prerender_should_wait_for_ready_title_;
  scoped_ptr<ui_test_utils::TitleWatcher> ready_title_watcher_;
};

// PrerenderManager that uses TestPrerenderContents.
class WaitForLoadPrerenderContentsFactory : public PrerenderContents::Factory {
 public:
  WaitForLoadPrerenderContentsFactory(
      int expected_number_of_loads,
      const std::deque<FinalStatus>& expected_final_status_queue,
      bool prerender_should_wait_for_ready_title)
      : expected_number_of_loads_(expected_number_of_loads),
        expected_final_status_queue_(expected_final_status_queue),
        prerender_should_wait_for_ready_title_(
            prerender_should_wait_for_ready_title) {
    VLOG(1) << "Factory created with queue length " <<
               expected_final_status_queue_.size();
  }

  virtual PrerenderContents* CreatePrerenderContents(
      PrerenderManager* prerender_manager,
      PrerenderTracker* prerender_tracker,
      Profile* profile,
      const GURL& url,
      const content::Referrer& referrer,
      Origin origin,
      uint8 experiment_id) OVERRIDE {
    FinalStatus expected_final_status = FINAL_STATUS_MAX;
    if (!expected_final_status_queue_.empty()) {
      expected_final_status = expected_final_status_queue_.front();
      expected_final_status_queue_.pop_front();
    }
    VLOG(1) << "Creating prerender contents for " << url.path() <<
               " with expected final status " << expected_final_status;
    VLOG(1) << expected_final_status_queue_.size() << " left in the queue.";
    return new TestPrerenderContents(prerender_manager, prerender_tracker,
                                     profile, url,
                                     referrer, expected_number_of_loads_,
                                     expected_final_status,
                                     prerender_should_wait_for_ready_title_);
  }

 private:
  int expected_number_of_loads_;
  std::deque<FinalStatus> expected_final_status_queue_;
  bool prerender_should_wait_for_ready_title_;
};

#if defined(ENABLE_SAFE_BROWSING)
// A SafeBrowingService implementation that returns a fixed result for a given
// URL.
class FakeSafeBrowsingService :  public SafeBrowsingService {
 public:
  FakeSafeBrowsingService() :
      result_(SAFE) {}

  // Called on the IO thread to check if the given url is safe or not.  If we
  // can synchronously determine that the url is safe, CheckUrl returns true.
  // Otherwise it returns false, and "client" is called asynchronously with the
  // result when it is ready.
  // Returns true, indicating a SAFE result, unless the URL is the fixed URL
  // specified by the user, and the user-specified result is not SAFE
  // (in which that result will be communicated back via a call into the
  // client, and false will be returned).
  // Overrides SafeBrowsingService::CheckBrowseUrl.
  virtual bool CheckBrowseUrl(const GURL& gurl, Client* client) OVERRIDE {
    if (gurl != url_ || result_ == SAFE)
      return true;

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&FakeSafeBrowsingService::OnCheckBrowseURLDone, this, gurl,
                   client));
    return false;
  }

  void SetResultForUrl(const GURL& url, UrlCheckResult result) {
    url_ = url;
    result_ = result;
  }

 private:
  virtual ~FakeSafeBrowsingService() {}

  void OnCheckBrowseURLDone(const GURL& gurl, Client* client) {
    SafeBrowsingService::SafeBrowsingCheck check;
    check.urls.push_back(gurl);
    check.client = client;
    check.result = result_;
    client->OnSafeBrowsingResult(check);
  }

  GURL url_;
  UrlCheckResult result_;
};

// Factory that creates FakeSafeBrowsingService instances.
class TestSafeBrowsingServiceFactory : public SafeBrowsingServiceFactory {
 public:
  TestSafeBrowsingServiceFactory() :
      most_recent_service_(NULL) { }
  virtual ~TestSafeBrowsingServiceFactory() { }

  virtual SafeBrowsingService* CreateSafeBrowsingService() OVERRIDE {
    most_recent_service_ =  new FakeSafeBrowsingService();
    return most_recent_service_;
  }

  FakeSafeBrowsingService* most_recent_service() const {
    return most_recent_service_;
  }

 private:
  FakeSafeBrowsingService* most_recent_service_;
};
#endif

class FakeDevToolsClientHost : public DevToolsClientHost {
 public:
  FakeDevToolsClientHost() {}
  virtual ~FakeDevToolsClientHost() {}
  virtual void InspectedContentsClosing() OVERRIDE {}
  virtual void FrameNavigating(const std::string& url) OVERRIDE {}
  virtual void DispatchOnInspectorFrontend(const std::string& msg) OVERRIDE {}
  virtual void ContentsReplaced(WebContents* new_contents) OVERRIDE {}
};

class RestorePrerenderMode {
 public:
  RestorePrerenderMode() : prev_mode_(PrerenderManager::GetMode()) {
  }

  ~RestorePrerenderMode() { PrerenderManager::SetMode(prev_mode_); }
 private:
  PrerenderManager::PrerenderManagerMode prev_mode_;
};

}  // namespace

class PrerenderBrowserTest : public InProcessBrowserTest {
 public:
  PrerenderBrowserTest()
      : prerender_contents_factory_(NULL),
#if defined(ENABLE_SAFE_BROWSING)
        safe_browsing_factory_(new TestSafeBrowsingServiceFactory()),
#endif
        use_https_src_server_(false),
        call_javascript_(true),
        loader_path_("files/prerender/prerender_loader.html"),
        explicitly_set_browser_(NULL) {
    EnableDOMAutomation();
  }

  virtual ~PrerenderBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
#if defined(ENABLE_SAFE_BROWSING)
    SafeBrowsingService::RegisterFactory(safe_browsing_factory_.get());
#endif
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kPrerenderMode,
                                    switches::kPrerenderModeSwitchValueEnabled);
#if defined(OS_MACOSX)
    // The plugins directory isn't read by default on the Mac, so it needs to be
    // explicitly registered.
    FilePath app_dir;
    PathService::Get(chrome::DIR_APP, &app_dir);
    command_line->AppendSwitchPath(
        switches::kExtraPluginDir,
        app_dir.Append(FILE_PATH_LITERAL("plugins")));
#endif
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    current_browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kPromptForDownload, false);
    IncreasePrerenderMemory();
    ASSERT_TRUE(test_server()->Start());
  }

  // Overload for a single expected final status
  void PrerenderTestURL(const std::string& html_file,
                        FinalStatus expected_final_status,
                        int expected_number_of_loads) {
    PrerenderTestURL(html_file,
                     expected_final_status,
                     expected_number_of_loads,
                     false);
  }

  void PrerenderTestURL(const std::string& html_file,
                        FinalStatus expected_final_status,
                        int expected_number_of_loads,
                        bool prerender_should_wait_for_ready_title) {
    std::deque<FinalStatus> expected_final_status_queue(1,
                                                        expected_final_status);
    PrerenderTestURL(html_file,
                     expected_final_status_queue,
                     expected_number_of_loads,
                     prerender_should_wait_for_ready_title);
  }

  void PrerenderTestURL(
      const std::string& html_file,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads,
      bool prerender_should_wait_for_ready_title) {
    GURL url = test_server()->GetURL(html_file);
    PrerenderTestURLImpl(url, url,
                         expected_final_status_queue,
                         expected_number_of_loads,
                         prerender_should_wait_for_ready_title);
  }

  void PrerenderTestURL(
      const std::string& html_file,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads) {
    PrerenderTestURL(html_file, expected_final_status_queue,
                     expected_number_of_loads, false);
  }

  void PrerenderTestURL(
      const GURL& url,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    std::deque<FinalStatus> expected_final_status_queue(1,
                                                        expected_final_status);
    PrerenderTestURLImpl(url, url,
                         expected_final_status_queue,
                         expected_number_of_loads,
                         false);
  }

  void PrerenderTestURL(
      const GURL& prerender_url,
      const GURL& destination_url,
      FinalStatus expected_final_status,
      int expected_number_of_loads) {
    std::deque<FinalStatus> expected_final_status_queue(1,
                                                        expected_final_status);
    PrerenderTestURLImpl(prerender_url, destination_url,
                         expected_final_status_queue,
                         expected_number_of_loads,
                         false);
  }

  void NavigateToDestURL() const {
    NavigateToDestURLWithDisposition(CURRENT_TAB);
  }

  // Opens the url in a new tab, with no opener.
  void NavigateToDestURLWithDisposition(
      WindowOpenDisposition disposition) const {
    NavigateToURLImpl(dest_url_, disposition);
  }

  void OpenDestURLViaClick() const {
    OpenDestURLWithJSImpl("Click()");
  }

  void OpenDestURLViaClickTarget() const {
    OpenDestURLWithJSImpl("ClickTarget()");
  }

  void OpenDestURLViaClickNewWindow() const {
    OpenDestURLWithJSImpl("ShiftClick()");
  }

  void OpenDestURLViaClickNewForegroundTab() const {
#if defined(OS_MACOSX)
    OpenDestURLWithJSImpl("MetaShiftClick()");
#else
    OpenDestURLWithJSImpl("CtrlShiftClick()");
#endif
  }

  void OpenDestURLViaClickNewBackgroundTab() const {
    TestPrerenderContents* prerender_contents = GetPrerenderContents();
    ASSERT_TRUE(prerender_contents != NULL);
    prerender_contents->set_should_be_shown(false);
#if defined(OS_MACOSX)
    OpenDestURLWithJSImpl("MetaClick()");
#else
    OpenDestURLWithJSImpl("CtrlClick()");
#endif
  }

  void OpenDestURLViaWindowOpen() const {
    OpenDestURLWithJSImpl("WindowOpen()");
  }

  void RemoveLinkElementsAndNavigate() const {
    OpenDestURLWithJSImpl("RemoveLinkElementsAndNavigate()");
  }

  void ClickToNextPageAfterPrerender(Browser* browser) {
    ui_test_utils::WindowedNotificationObserver new_page_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    RenderViewHost* render_view_host =
        browser->GetSelectedWebContents()->GetRenderViewHost();
    render_view_host->ExecuteJavascriptInWebFrame(
        string16(),
        ASCIIToUTF16("ClickOpenLink()"));
    new_page_observer.Wait();
  }

  void NavigateToNextPageAfterPrerender(Browser* browser) {
    ui_test_utils::NavigateToURL(
        browser,
        test_server()->GetURL("files/prerender/prerender_page.html"));
  }

  void NavigateToDestUrlAndWaitForPassTitle() {
    string16 expected_title = ASCIIToUTF16(kPassTitle);
    ui_test_utils::TitleWatcher title_watcher(
        GetPrerenderContents()->prerender_contents()->web_contents(),
        expected_title);
    NavigateToDestURL();
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  // Called after the prerendered page has been navigated to and then away from.
  // Navigates back through the history to the prerendered page.
  void GoBackToPrerender(Browser* browser) {
    ui_test_utils::WindowedNotificationObserver back_nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
    browser->GoBack(CURRENT_TAB);
    back_nav_observer.Wait();
    bool original_prerender_page = false;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        browser->GetSelectedWebContents()->GetRenderViewHost(), L"",
        L"window.domAutomationController.send(IsOriginalPrerenderPage())",
        &original_prerender_page));
    EXPECT_TRUE(original_prerender_page);
  }

  // Goes back to the page that was active before the prerender was swapped
  // in. This must be called when the prerendered page is the current page
  // in the active tab.
  void GoBackToPageBeforePrerender(Browser* browser) {
    WebContents* tab = browser->GetSelectedWebContents();
    ASSERT_TRUE(tab);
    EXPECT_FALSE(tab->IsLoading());
    ui_test_utils::WindowedNotificationObserver back_nav_observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&tab->GetController()));
    browser->GoBack(CURRENT_TAB);
    back_nav_observer.Wait();
    bool js_result;
    ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
        tab->GetRenderViewHost(), L"",
        L"window.domAutomationController.send(DidBackToOriginalPagePass())",
        &js_result));
    EXPECT_TRUE(js_result);
  }

  // Should be const but test_server()->GetURL(...) is not const.
  void NavigateToURL(const std::string& dest_html_file) {
    GURL dest_url = test_server()->GetURL(dest_html_file);
    NavigateToURLImpl(dest_url, CURRENT_TAB);
  }

  bool UrlIsInPrerenderManager(const std::string& html_file) {
    GURL dest_url = test_server()->GetURL(html_file);
    return (GetPrerenderManager()->FindEntry(dest_url) != NULL);
  }

  bool UrlIsInPrerenderManager(const GURL& url) {
    return (GetPrerenderManager()->FindEntry(url) != NULL);
  }

  bool UrlIsPendingInPrerenderManager(const std::string& html_file) {
    GURL dest_url = test_server()->GetURL(html_file);
    return GetPrerenderManager()->IsPendingEntry(dest_url);
  }

  void set_use_https_src(bool use_https_src_server) {
    use_https_src_server_ = use_https_src_server;
  }

  void DisableJavascriptCalls() {
    call_javascript_ = false;
  }

  TaskManagerModel* GetModel() const {
    return TaskManager::GetInstance()->model();
  }

  PrerenderManager* GetPrerenderManager() const {
    Profile* profile =
        current_browser()->GetSelectedTabContentsWrapper()->profile();
    PrerenderManager* prerender_manager =
        PrerenderManagerFactory::GetForProfile(profile);
    return prerender_manager;
  }

  const PrerenderLinkManager* GetPrerenderLinkManager() const {
    Profile* profile =
        current_browser()->GetSelectedTabContentsWrapper()->profile();
    PrerenderLinkManager* prerender_link_manager =
        PrerenderLinkManagerFactory::GetForProfile(profile);
    return prerender_link_manager;
  }

  bool IsEmptyPrerenderLinkManager() const {
    return GetPrerenderLinkManager()->IsEmpty();
  }

  // Returns length of |prerender_manager_|'s history, or -1 on failure.
  int GetHistoryLength() const {
    scoped_ptr<DictionaryValue> prerender_dict(
        static_cast<DictionaryValue*>(GetPrerenderManager()->GetAsValue()));
    if (!prerender_dict.get())
      return -1;
    ListValue* history_list;
    if (!prerender_dict->GetList("history", &history_list))
      return -1;
    return static_cast<int>(history_list->GetSize());
  }

#if defined(ENABLE_SAFE_BROWSING)
  FakeSafeBrowsingService* GetSafeBrowsingService() {
    return safe_browsing_factory_->most_recent_service();
  }
#endif

  TestPrerenderContents* GetPrerenderContents() const {
    return static_cast<TestPrerenderContents*>(
        GetPrerenderManager()->FindEntry(dest_url_));
  }

  void set_loader_path(const std::string& path) {
    loader_path_ = path;
  }

  void set_loader_query_and_fragment(const std::string& query_and_fragment) {
    loader_query_and_fragment_ = query_and_fragment;
  }

  GURL GetCrossDomainTestUrl(const std::string& path) {
    static const std::string secondary_domain = "www.foo.com";
    host_resolver()->AddRule(secondary_domain, "127.0.0.1");
    std::string url_str(base::StringPrintf(
        "http://%s:%d/%s",
        secondary_domain.c_str(),
        test_server()->host_port_pair().port(),
        path.c_str()));
    return GURL(url_str);
  }

  void set_browser(Browser* browser) {
    explicitly_set_browser_ = browser;
  }

  Browser* current_browser() const {
    if (explicitly_set_browser_)
      return explicitly_set_browser_;
    else
      return browser();
  }

  void IncreasePrerenderMemory() {
    // Increase the memory allowed in a prerendered page above normal settings.
    // Debug build bots occasionally run against the default limit, and tests
    // were failing because the prerender was canceled due to memory exhaustion.
    // http://crbug.com/93076
    GetPrerenderManager()->mutable_config().max_bytes = 1000 * 1024 * 1024;
  }

 private:
  void PrerenderTestURLImpl(
      const GURL& prerender_url,
      const GURL& destination_url,
      const std::deque<FinalStatus>& expected_final_status_queue,
      int expected_number_of_loads,
      bool prerender_should_wait_for_ready_title) {
    dest_url_ = destination_url;

    std::vector<net::TestServer::StringPair> replacement_text;
    replacement_text.push_back(
        make_pair("REPLACE_WITH_PRERENDER_URL", prerender_url.spec()));
    replacement_text.push_back(
        make_pair("REPLACE_WITH_DESTINATION_URL", destination_url.spec()));
    std::string replacement_path;
    ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
        loader_path_,
        replacement_text,
        &replacement_path));

    net::TestServer* src_server = test_server();
    scoped_ptr<net::TestServer> https_src_server;
    if (use_https_src_server_) {
      https_src_server.reset(
          new net::TestServer(net::TestServer::TYPE_HTTPS,
                              net::TestServer::kLocalhost,
                              FilePath(FILE_PATH_LITERAL("chrome/test/data"))));
      ASSERT_TRUE(https_src_server->Start());
      src_server = https_src_server.get();
    }
    GURL loader_url = src_server->GetURL(replacement_path +
                                         loader_query_and_fragment_);

    PrerenderManager* prerender_manager = GetPrerenderManager();
    ASSERT_TRUE(prerender_manager);
    prerender_manager->mutable_config().rate_limit_enabled = false;
    prerender_manager->mutable_config().https_allowed = true;
    ASSERT_TRUE(prerender_contents_factory_ == NULL);
    prerender_contents_factory_ =
        new WaitForLoadPrerenderContentsFactory(
            expected_number_of_loads,
            expected_final_status_queue,
            prerender_should_wait_for_ready_title);
    prerender_manager->SetPrerenderContentsFactory(
        prerender_contents_factory_);
    FinalStatus expected_final_status = expected_final_status_queue.front();

    // ui_test_utils::NavigateToURL uses its own observer and message loop.
    // Since the test needs to wait until the prerendered page has stopped
    // loading, rather than the page directly navigated to, need to
    // handle browser navigation directly.
    current_browser()->OpenURL(OpenURLParams(
        loader_url, Referrer(), CURRENT_TAB, content::PAGE_TRANSITION_TYPED,
        false));

    ui_test_utils::RunMessageLoop();

    TestPrerenderContents* prerender_contents = GetPrerenderContents();

    if (ShouldRenderPrerenderedPageCorrectly(expected_final_status)) {
      ASSERT_TRUE(prerender_contents != NULL);
      EXPECT_EQ(FINAL_STATUS_MAX, prerender_contents->final_status());

      if (call_javascript_ && expected_number_of_loads > 0) {
        // Wait for the prerendered page to change title to signal it is ready
        // if required.
        prerender_contents->WaitForPrerenderToHaveReadyTitleIfRequired();

        // Check if page behaves as expected while in prerendered state.
        bool prerender_test_result = false;
        ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
            prerender_contents->GetRenderViewHostMutable(), L"",
            L"window.domAutomationController.send(DidPrerenderPass())",
            &prerender_test_result));
        EXPECT_TRUE(prerender_test_result);
      }
    } else {
      // In the failure case, we should have removed |dest_url_| from the
      // prerender_manager.  We ignore dummy PrerenderContents (as indicated
      // by not having started).
      EXPECT_TRUE(prerender_contents == NULL ||
                  !prerender_contents->prerendering_has_started());
    }
  }

  void NavigateToURLImpl(const GURL& dest_url,
                         WindowOpenDisposition disposition) const {
    ASSERT_TRUE(GetPrerenderManager() != NULL);
    // Make sure in navigating we have a URL to use in the PrerenderManager.
    ASSERT_TRUE(GetPrerenderContents() != NULL);

    // If opening the page in a background tab, it won't be shown when swapped
    // in.
    if (disposition == NEW_BACKGROUND_TAB)
      GetPrerenderContents()->set_should_be_shown(false);

    scoped_ptr<ui_test_utils::WindowedNotificationObserver> page_load_observer;
    WebContents* web_contents = NULL;

    if (GetPrerenderContents()->prerender_contents()) {
      // In the case of zero loads, need to wait for the page load to complete
      // before running any Javascript.
      web_contents =
          GetPrerenderContents()->prerender_contents()->web_contents();
      if (GetPrerenderContents()->number_of_loads() == 0) {
        page_load_observer.reset(
            new ui_test_utils::WindowedNotificationObserver(
                content::NOTIFICATION_LOAD_STOP,
                content::Source<NavigationController>(
                    &web_contents->GetController())));
      }
    }

    // Navigate to the prerendered URL, but don't run the message loop. Browser
    // issued navigations to prerendered pages will synchronously swap in the
    // prerendered page.
    ui_test_utils::NavigateToURLWithDisposition(
        current_browser(), dest_url, disposition,
        ui_test_utils::BROWSER_TEST_NONE);

    // Make sure the PrerenderContents found earlier was used or removed.
    EXPECT_TRUE(GetPrerenderContents() == NULL);

    if (call_javascript_ && web_contents) {
      if (page_load_observer.get())
        page_load_observer->Wait();

      bool display_test_result = false;
      ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
          web_contents->GetRenderViewHost(), L"",
          L"window.domAutomationController.send(DidDisplayPass())",
          &display_test_result));
      EXPECT_TRUE(display_test_result);
    }
  }

  // Opens the prerendered page using javascript functions in the
  // loader page. |javascript_function_name| should be a 0 argument function
  // which is invoked.
  void OpenDestURLWithJSImpl(const std::string& javascript_function_name)
      const {
    TestPrerenderContents* prerender_contents = GetPrerenderContents();
    ASSERT_TRUE(prerender_contents != NULL);

    RenderViewHost* render_view_host =
        current_browser()->GetSelectedWebContents()->GetRenderViewHost();
    render_view_host->ExecuteJavascriptInWebFrame(
        string16(),
        ASCIIToUTF16(javascript_function_name));

    // Run message loop until the prerender contents is destroyed.
    ui_test_utils::RunMessageLoop();
  }

  WaitForLoadPrerenderContentsFactory* prerender_contents_factory_;
#if defined(ENABLE_SAFE_BROWSING)
  scoped_ptr<TestSafeBrowsingServiceFactory> safe_browsing_factory_;
#endif
  GURL dest_url_;
  bool use_https_src_server_;
  bool call_javascript_;
  std::string loader_path_;
  std::string loader_query_and_fragment_;
  Browser* explicitly_set_browser_;
};

// Checks that a page is correctly prerendered in the case of a
// <link rel=prerender> tag and then loaded into a tab in response to a
// navigation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPage) {
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// TODO(gavinp): After https://bugs.webkit.org/show_bug.cgi?id=85005 lands,
// enable this test.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderPageRemovingLink) {
  set_loader_path("files/prerender/prerender_loader_removing_links.html");
  set_loader_query_and_fragment("?links_to_insert=1&links_to_remove=1");
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CANCELLED, 1);
  RemoveLinkElementsAndNavigate();
  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// TODO(gavinp): After https://bugs.webkit.org/show_bug.cgi?id=85005 lands,
// enable this test.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderPageRemovingLinkWithTwoLinks) {
  set_loader_path("files/prerender/prerender_loader_removing_links.html");
  set_loader_query_and_fragment("?links_to_insert=2&links_to_remove=2");
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CANCELLED, 1);
  RemoveLinkElementsAndNavigate();
  ASSERT_TRUE(IsEmptyPrerenderLinkManager());
}

// TODO(gavinp): After https://bugs.webkit.org/show_bug.cgi?id=85005 lands,
// enable this test.
IN_PROC_BROWSER_TEST_F(
    PrerenderBrowserTest,
    DISABLED_PrerenderPageRemovingLinkWithTwoLinksRemovingOne) {
  set_loader_path("files/prerender/prerender_loader_removing_links.html");
  set_loader_query_and_fragment("?links_to_insert=2&links_to_remove=1");
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_USED, 1);
  RemoveLinkElementsAndNavigate();
}

// Checks that prerendering works in incognito mode.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderIncognito) {
  Profile* normal_profile = current_browser()->profile();
  ui_test_utils::OpenURLOffTheRecord(normal_profile, GURL("about:blank"));
  set_browser(BrowserList::FindBrowserWithProfile(
      normal_profile->GetOffTheRecordProfile()));
  // Increase memory expectations on the incognito PrerenderManager.
  IncreasePrerenderMemory();
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that the visibility API works.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderVisibility) {
  PrerenderTestURL("files/prerender/prerender_visibility.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the visibility API works when the prerender is quickly opened
// in a new tab before it stops loading.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderVisibilityQuickSwitch) {
  PrerenderTestURL("files/prerender/prerender_visibility_quick.html",
                   FINAL_STATUS_USED, 0);
  NavigateToDestURL();
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertBeforeOnload) {
  PrerenderTestURL("files/prerender/prerender_alert_before_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT,
                   1);
}

// Checks that the prerendering of a page is canceled correctly when a
// Javascript alert is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderAlertAfterOnload) {
  PrerenderTestURL("files/prerender/prerender_alert_after_onload.html",
                   FINAL_STATUS_JAVASCRIPT_ALERT,
                   1);
}

// Checks that plugins are not loaded while a page is being preloaded, but
// are loaded when the page is displayed.
#if defined(USE_AURA)
// http://crbug.com/103496
#define MAYBE_PrerenderDelayLoadPlugin DISABLED_PrerenderDelayLoadPlugin
#else
#define MAYBE_PrerenderDelayLoadPlugin PrerenderDelayLoadPlugin
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MAYBE_PrerenderDelayLoadPlugin) {
  PrerenderTestURL("files/prerender/plugin_delay_load.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that plugins are not loaded on prerendering pages when click-to-play
// is enabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickToPlay) {
  // Enable click-to-play.
  HostContentSettingsMap* content_settings_map =
      current_browser()->profile()->GetHostContentSettingsMap();
  content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_ASK);

  PrerenderTestURL("files/prerender/prerender_plugin_click_to_play.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that we don't load a NaCl plugin when NaCl is disabled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNaClPluginDisabled) {
  PrerenderTestURL("files/prerender/prerender_plugin_nacl_disabled.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();


  // Run this check again.  When we try to load aa ppapi plugin, the
  // "loadstart" event is asynchronously posted to a message loop.
  // It's possible that earlier call could have been run before the
  // the "loadstart" event was posted.
  // TODO(mmenke):  While this should reliably fail on regressions, the
  //                reliability depends on the specifics of ppapi plugin
  //                loading.  It would be great if we could avoid that.
  WebContents* web_contents = browser()->GetSelectedWebContents();
  bool display_test_result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      web_contents->GetRenderViewHost(), L"",
      L"window.domAutomationController.send(DidDisplayPass())",
      &display_test_result));
  EXPECT_TRUE(display_test_result);
}

// Checks that plugins in an iframe are not loaded while a page is
// being preloaded, but are loaded when the page is displayed.
#if defined(USE_AURA)
// http://crbug.com/103496
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        DISABLED_PrerenderIframeDelayLoadPlugin
#elif defined(OS_MACOSX)
// http://crbug.com/100514
#define MAYBE_PrerenderIframeDelayLoadPlugin \
        FAILS_PrerenderIframeDelayLoadPlugin
#else
#define MAYBE_PrerenderIframeDelayLoadPlugin PrerenderIframeDelayLoadPlugin
#endif
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       MAYBE_PrerenderIframeDelayLoadPlugin) {
  PrerenderTestURL("files/prerender/prerender_iframe_plugin_delay_load.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Renders a page that contains a prerender link to a page that contains an
// iframe with a source that requires http authentication. This should not
// prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttpAuthentication) {
  PrerenderTestURL("files/prerender/prerender_http_auth_container.html",
                   FINAL_STATUS_AUTH_NEEDED,
                   1);
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToFirst) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   2);
  NavigateToDestURL();
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecond) {
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   2);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that client-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection via a mouse click.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectNavigateToSecondViaClick) {
  GURL prerender_url = test_server()->GetURL(
      CreateClientRedirect("files/prerender/prerender_page.html"));
  GURL destination_url = test_server()->GetURL(
      "files/prerender/prerender_page.html");
  PrerenderTestURL(prerender_url, destination_url, FINAL_STATUS_USED, 2);
  OpenDestURLViaClick();
}

// Checks that a prerender for an https will prevent a prerender from happening.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               net::TestServer::kLocalhost,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that client-issued redirects to an https page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClientRedirectToHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               net::TestServer::kLocalhost,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(CreateClientRedirect(https_url.spec()),
                   FINAL_STATUS_USED,
                   2);
  NavigateToDestURL();
}

// Checks that client-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClientRedirectInIframe) {
  std::string redirect_path = CreateClientRedirect(
      "/files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 2);
  EXPECT_FALSE(UrlIsInPrerenderManager(
      "files/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that client-issued redirects within an iframe in a prerendered
// page to an https page will not cancel the prerender, nor will it
// count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderClientRedirectToHttpsInIframe) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               net::TestServer::kLocalhost,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  std::string redirect_path = CreateClientRedirect(https_url.spec());
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 2);
  EXPECT_FALSE(UrlIsInPrerenderManager(https_url));
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the page which issues the redirection, rather
// than the final destination page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToFirst) {
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecond) {
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_page.html");
}

// Checks that server-issued redirects work with prerendering.
// This version navigates to the final destination page, rather than the
// page which does the redirection via a mouse click.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectNavigateToSecondViaClick) {
  GURL prerender_url = test_server()->GetURL(
      CreateServerRedirect("files/prerender/prerender_page.html"));
  GURL destination_url = test_server()->GetURL(
      "files/prerender/prerender_page.html");
  PrerenderTestURL(prerender_url, destination_url, FINAL_STATUS_USED, 1);
  OpenDestURLViaClick();
}

// Checks that server-issued redirects from an http to an https
// location will cancel prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectToHttps) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               net::TestServer::kLocalhost,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(CreateServerRedirect(https_url.spec()),
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that server-issued redirects within an iframe in a prerendered
// page will not count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderServerRedirectInIframe) {
  std::string redirect_path = CreateServerRedirect(
      "/files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(
      "files/prerender/prerender_embedded_content.html"));
  NavigateToDestURL();
}

// Checks that server-issued redirects within an iframe in a prerendered
// page to an https page will not cancel the prerender, nor will it
// count as an "alias" for the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderServerRedirectToHttpsInIframe) {
  net::TestServer https_server(net::TestServer::TYPE_HTTPS,
                               net::TestServer::kLocalhost,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  std::string redirect_path = CreateServerRedirect(https_url.spec());
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", "/" + redirect_path));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  EXPECT_FALSE(UrlIsInPrerenderManager(https_url));
  NavigateToDestURL();
}

// Prerenders a page that contains an automatic download triggered through an
// iframe. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadIframe) {
  PrerenderTestURL("files/prerender/prerender_download_iframe.html",
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through
// Javascript changing the window.location. This should not prerender
// successfully
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadLocation) {
  PrerenderTestURL(CreateClientRedirect("files/download-test1.lib"),
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Prerenders a page that contains an automatic download triggered through a
// client-issued redirect. This should not prerender successfully.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderDownloadClientRedirect) {
  PrerenderTestURL("files/prerender/prerender_download_refresh.html",
                   FINAL_STATUS_DOWNLOAD,
                   1);
}

// Checks that the referrer is set when prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderReferrer) {
  PrerenderTestURL("files/prerender/prerender_referrer.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the referrer is not set when prerendering and the source page is
// HTTPS.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNoSSLReferrer) {
  set_use_https_src(true);
  PrerenderTestURL("files/prerender/prerender_no_referrer.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that popups on a prerendered page cause cancellation.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPopup) {
  PrerenderTestURL("files/prerender/prerender_popup.html",
                   FINAL_STATUS_CREATE_NEW_WINDOW,
                   1);
}

// Checks that renderers using excessive memory will be terminated.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderExcessiveMemory) {
  ASSERT_TRUE(GetPrerenderManager());
  GetPrerenderManager()->mutable_config().max_bytes = 30 * 1024 * 1024;
  PrerenderTestURL("files/prerender/prerender_excessive_memory.html",
                   FINAL_STATUS_MEMORY_LIMIT_EXCEEDED,
                   1);
}

// Checks shutdown code while a prerender is active.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderQuickQuit) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_APP_TERMINATING,
                   0);
}

// Checks that we don't prerender in an infinite loop.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderInfiniteLoop) {
  const char* const kHtmlFileA = "files/prerender/prerender_infinite_a.html";
  const char* const kHtmlFileB = "files/prerender/prerender_infinite_b.html";

  std::deque<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);
  ASSERT_TRUE(GetPrerenderContents());
  GetPrerenderContents()->WaitForPendingPrerenders(1u);

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileB));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next url is now in the manager
  // and not pending.
  EXPECT_TRUE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileB));
}

// Checks that we don't prerender in an infinite loop and multiple links are
// handled correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderInfiniteLoopMultiple) {
  const char* const kHtmlFileA =
      "files/prerender/prerender_infinite_a_multiple.html";
  const char* const kHtmlFileB =
      "files/prerender/prerender_infinite_b_multiple.html";
  const char* const kHtmlFileC =
      "files/prerender/prerender_infinite_c_multiple.html";

  // We need to set the final status to expect here before starting any
  // prerenders. We set them on a queue so whichever we see first is expected to
  // be evicted, and the second should stick around until we exit.
  std::deque<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_USED);
  expected_final_status_queue.push_back(FINAL_STATUS_EVICTED);
  expected_final_status_queue.push_back(FINAL_STATUS_APP_TERMINATING);

  PrerenderTestURL(kHtmlFileA, expected_final_status_queue, 1);
  ASSERT_TRUE(GetPrerenderContents());
  GetPrerenderContents()->WaitForPendingPrerenders(2u);

  // Next url should be in pending list but not an active entry.
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsInPrerenderManager(kHtmlFileC));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileB));
  EXPECT_TRUE(UrlIsPendingInPrerenderManager(kHtmlFileC));

  NavigateToDestURL();

  // Make sure the PrerenderContents for the next urls are now in the manager
  // and not pending. One and only one of the URLs (the last seen) should be the
  // active entry.
  bool url_b_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileB);
  bool url_c_is_active_prerender = UrlIsInPrerenderManager(kHtmlFileC);
  EXPECT_TRUE((url_b_is_active_prerender || url_c_is_active_prerender) &&
              !(url_b_is_active_prerender && url_c_is_active_prerender));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileB));
  EXPECT_FALSE(UrlIsPendingInPrerenderManager(kHtmlFileC));
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderTaskManager) {
  // Show the task manager. This populates the model.
  current_browser()->window()->ShowTaskManager();
  // Wait for the model of task manager to start.
  TaskManagerBrowserTestUtil::WaitForResourceChange(2);

  // Start with two resources.
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);

  // One of the resources that has a WebContents associated with it should have
  // the Prerender prefix.
  const string16 prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRERENDER_PREFIX, string16());
  string16 prerender_title;
  int num_prerender_tabs = 0;

  const TaskManagerModel* model = GetModel();
  for (int i = 0; i < model->ResourceCount(); ++i) {
    if (model->GetResourceTabContents(i)) {
      prerender_title = model->GetResourceTitle(i);
      if (StartsWith(prerender_title, prefix, true))
        ++num_prerender_tabs;
    }
  }
  EXPECT_EQ(1, num_prerender_tabs);
  const string16 prerender_page_title = prerender_title.substr(prefix.length());

  NavigateToDestURL();

  // There should be no tabs with the Prerender prefix.
  const string16 tab_prefix =
      l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_TAB_PREFIX, string16());
  num_prerender_tabs = 0;
  int num_tabs_with_prerender_page_title = 0;
  for (int i = 0; i < model->ResourceCount(); ++i) {
    if (model->GetResourceTabContents(i)) {
      string16 tab_title = model->GetResourceTitle(i);
      if (StartsWith(tab_title, prefix, true)) {
        ++num_prerender_tabs;
      } else {
        EXPECT_TRUE(StartsWith(tab_title, tab_prefix, true));

        // The prerender tab should now be a normal tab but the title should be
        // the same. Depending on timing, there may be more than one of these.
        const string16 tab_page_title = tab_title.substr(tab_prefix.length());
        if (prerender_page_title.compare(tab_page_title) == 0)
          ++num_tabs_with_prerender_page_title;
      }
    }
  }
  EXPECT_EQ(0, num_prerender_tabs);

  // We may have deleted the prerender tab, but the swapped in tab should be
  // active.
  EXPECT_GE(num_tabs_with_prerender_page_title, 1);
  EXPECT_LE(num_tabs_with_prerender_page_title, 2);
}

// Checks that audio loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5Audio) {
  PrerenderTestURL("files/prerender/prerender_html5_audio.html",
                  FINAL_STATUS_USED,
                  1);
  NavigateToDestUrlAndWaitForPassTitle();
}

// Checks that audio loads are deferred on prerendering and played back when
// the prerender is swapped in if autoplay is set.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5AudioAutoplay) {
  PrerenderTestURL("files/prerender/prerender_html5_audio_autoplay.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestUrlAndWaitForPassTitle();
}

// Checks that audio loads are deferred on prerendering and played back when
// the prerender is swapped in if js starts playing.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5AudioJsplay) {
  PrerenderTestURL("files/prerender/prerender_html5_audio_jsplay.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestUrlAndWaitForPassTitle();
}

// Checks that video loads are deferred on prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5Video) {
  PrerenderTestURL("files/prerender/prerender_html5_video.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestUrlAndWaitForPassTitle();
}

// Checks that video tags inserted by javascript are deferred and played
// correctly on swap in.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5VideoJs) {
  PrerenderTestURL("files/prerender/prerender_html5_video_script.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestUrlAndWaitForPassTitle();
}

// Checks for correct network events by using a busy sleep the javascript.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderHTML5VideoNetwork) {
  PrerenderTestURL("files/prerender/prerender_html5_video_network.html",
                   FINAL_STATUS_USED,
                   1,
                   true);
  NavigateToDestUrlAndWaitForPassTitle();
}

// Checks that scripts can retrieve the correct window size while prerendering.
#if defined(TOOLKIT_VIEWS)
// TODO(beng): Widget hierarchy split causes this to fail http://crbug.com/82363
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, DISABLED_PrerenderWindowSize) {
#else
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderWindowSize) {
#endif
  PrerenderTestURL("files/prerender/prerender_size.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that prerenderers will terminate when the RenderView crashes.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderRendererCrash) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_RENDERER_CRASHED,
                   1);

  // Navigate to about:crash and then wait for the renderer to crash.
  ASSERT_TRUE(GetPrerenderContents());
  ASSERT_TRUE(GetPrerenderContents()->prerender_contents());
  GetPrerenderContents()->prerender_contents()->web_contents()->GetController().
      LoadURL(
          GURL(chrome::kChromeUICrashURL),
          content::Referrer(),
          content::PAGE_TRANSITION_TYPED,
          std::string());
  ui_test_utils::RunMessageLoop();
}

// Checks that we correctly use a prerendered page when navigating to a
// fragment.
// DISABLED: http://crbug.com/84154
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderPageNavigateFragment) {
  PrerenderTestURL("files/prerender/prerender_fragment.html",
                   FINAL_STATUS_FRAGMENT_MISMATCH,
                   1);
  NavigateToURL("files/prerender/prerender_fragment.html#fragment");
}

// Checks that we correctly use a prerendered page when we prerender a fragment
// but navigate to the main page.
// http://crbug.com/83901
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderFragmentNavigatePage) {
  PrerenderTestURL("files/prerender/prerender_fragment.html#fragment",
                   FINAL_STATUS_FRAGMENT_MISMATCH,
                   1);
  NavigateToURL("files/prerender/prerender_fragment.html");
}

// Checks that we correctly use a prerendered page when we prerender a fragment
// but navigate to a different fragment on the same page.
// DISABLED: http://crbug.com/84154
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderFragmentNavigateFragment) {
  PrerenderTestURL("files/prerender/prerender_fragment.html#other_fragment",
                   FINAL_STATUS_FRAGMENT_MISMATCH,
                   1);
  NavigateToURL("files/prerender/prerender_fragment.html#fragment");
}

// Checks that we correctly use a prerendered page when the page uses a client
// redirect to refresh from a fragment on the same page.
// http://crbug.com/83901
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderClientRedirectFromFragment) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/prerender_fragment.html#fragment"),
      FINAL_STATUS_FRAGMENT_MISMATCH,
      2);
  NavigateToURL("files/prerender/prerender_fragment.html");
}

// Checks that we correctly use a prerendered page when the page uses a client
// redirect to refresh to a fragment on the same page.
// DISABLED: http://crbug.com/84154
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderClientRedirectToFragment) {
  PrerenderTestURL(
      CreateClientRedirect("files/prerender/prerender_fragment.html"),
      FINAL_STATUS_FRAGMENT_MISMATCH,
      2);
  NavigateToURL("files/prerender/prerender_fragment.html#fragment");
}

// Checks that we correctly use a prerendered page when the page uses JS to set
// the window.location.hash to a fragment on the same page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderPageChangeFragmentLocationHash) {
  PrerenderTestURL("files/prerender/prerender_fragment_location_hash.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToURL("files/prerender/prerender_fragment_location_hash.html");
}

// Checks that prerendering a PNG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImagePng) {
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/image.png", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that prerendering a JPG works correctly.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderImageJpeg) {
  DisableJavascriptCalls();
  PrerenderTestURL("files/prerender/image.jpeg", FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that a prerender of a CRX will result in a cancellation due to
// download.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCrx) {
  PrerenderTestURL("files/prerender/extension.crx", FINAL_STATUS_DOWNLOAD, 1);
}

// Checks that xhr GET requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrGet) {
  PrerenderTestURL("files/prerender/prerender_xhr_get.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr HEAD requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrHead) {
  PrerenderTestURL("files/prerender/prerender_xhr_head.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr OPTIONS requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrOptions) {
  PrerenderTestURL("files/prerender/prerender_xhr_options.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr TRACE requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrTrace) {
  PrerenderTestURL("files/prerender/prerender_xhr_trace.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr POST requests allow prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPost) {
  PrerenderTestURL("files/prerender/prerender_xhr_post.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that xhr PUT cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrPut) {
  PrerenderTestURL("files/prerender/prerender_xhr_put.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that xhr DELETE cancels prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderXhrDelete) {
  PrerenderTestURL("files/prerender/prerender_xhr_delete.html",
                   FINAL_STATUS_INVALID_HTTP_METHOD,
                   1);
}

// Checks that a top-level page which would trigger an SSL error is canceled.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorTopLevel) {
  net::TestServer::HTTPSOptions https_options;
  https_options.server_certificate =
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url,
                   FINAL_STATUS_SSL_ERROR,
                   1);
}

// Checks that an SSL error that comes from a subresource does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorSubresource) {
  net::TestServer::HTTPSOptions https_options;
  https_options.server_certificate =
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/image.jpeg");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that an SSL error that comes from an iframe does not cancel
// the page. Non-main-frame requests are simply cancelled if they run into
// an SSL problem.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLErrorIframe) {
  net::TestServer::HTTPSOptions https_options;
  https_options.server_certificate =
      net::TestServer::HTTPSOptions::CERT_MISMATCHED_NAME;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(
      "files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path, FINAL_STATUS_USED, 1);
  NavigateToDestURL();
}

// Checks that we cancel correctly when window.print() is called.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderPrint) {
  PrerenderTestURL("files/prerender/prerender_print.html",
                   FINAL_STATUS_WINDOW_PRINT,
                   1);
}

// Checks that if a page is opened in a new window by javascript and both the
// pages are in the same domain, the prerendered page is not used.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSameDomainWindowOpenerWindowOpen) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_WINDOW_OPENER,
                   1);
  OpenDestURLViaWindowOpen();
}

// Checks that if a page is opened due to click on a href with target="_blank"
// and both pages are in the same domain the prerendered page is not used.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSameDomainWindowOpenerClickTarget) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_WINDOW_OPENER,
                   1);
  OpenDestURLViaClickTarget();
}

// Checks that a top-level page which would normally request an SSL client
// certificate will never be seen since it's an https top-level resource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLClientCertTopLevel) {
  net::TestServer::HTTPSOptions https_options;
  https_options.request_client_certificate = true;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/prerender_page.html");
  PrerenderTestURL(https_url, FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED, 1);
}

// Checks that an SSL Client Certificate request that originates from a
// subresource will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSSLClientCertSubresource) {
  net::TestServer::HTTPSOptions https_options;
  https_options.request_client_certificate = true;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL("files/prerender/image.jpeg");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED,
                   1);
}

// Checks that an SSL Client Certificate request that originates from an
// iframe will cancel the prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSSLClientCertIframe) {
  net::TestServer::HTTPSOptions https_options;
  https_options.request_client_certificate = true;
  net::TestServer https_server(https_options,
                               FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(https_server.Start());
  GURL https_url = https_server.GetURL(
      "files/prerender/prerender_embedded_content.html");
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", https_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SSL_CLIENT_CERTIFICATE_REQUESTED,
                   1);
}

#if defined(ENABLE_SAFE_BROWSING)
// Ensures that we do not prerender pages with a safe browsing
// interstitial.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingTopLevel) {
  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  GetSafeBrowsingService()->SetResultForUrl(
      url, SafeBrowsingService::URL_MALWARE);
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_SAFE_BROWSING, 1);
}

// Ensures that server redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSafeBrowsingServerRedirect) {
  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  GetSafeBrowsingService()->SetResultForUrl(
      url, SafeBrowsingService::URL_MALWARE);
  PrerenderTestURL(CreateServerRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING,
                   1);
}

// Ensures that client redirects to a malware page will cancel prerenders.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       PrerenderSafeBrowsingClientRedirect) {
  GURL url = test_server()->GetURL("files/prerender/prerender_page.html");
  GetSafeBrowsingService()->SetResultForUrl(
      url, SafeBrowsingService::URL_MALWARE);
  PrerenderTestURL(CreateClientRedirect("files/prerender/prerender_page.html"),
                   FINAL_STATUS_SAFE_BROWSING,
                   1);
}

// Ensures that we do not prerender pages which have a malware subresource.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingSubresource) {
  GURL image_url = test_server()->GetURL("files/prerender/image.jpeg");
  GetSafeBrowsingService()->SetResultForUrl(
      image_url, SafeBrowsingService::URL_MALWARE);
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_IMAGE_URL", image_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_image.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SAFE_BROWSING,
                   1);
}

// Ensures that we do not prerender pages which have a malware iframe.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSafeBrowsingIframe) {
  GURL iframe_url = test_server()->GetURL(
      "files/prerender/prerender_embedded_content.html");
  GetSafeBrowsingService()->SetResultForUrl(
      iframe_url, SafeBrowsingService::URL_MALWARE);
  std::vector<net::TestServer::StringPair> replacement_text;
  replacement_text.push_back(
      std::make_pair("REPLACE_WITH_URL", iframe_url.spec()));
  std::string replacement_path;
  ASSERT_TRUE(net::TestServer::GetFilePathWithReplacements(
      "files/prerender/prerender_with_iframe.html",
      replacement_text,
      &replacement_path));
  PrerenderTestURL(replacement_path,
                   FINAL_STATUS_SAFE_BROWSING,
                   1);
}

#endif

// Checks that a local storage read will not cause prerender to fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderLocalStorageRead) {
  PrerenderTestURL("files/prerender/prerender_localstorage_read.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that a local storage write will not cause prerender to fail.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderLocalStorageWrite) {
  PrerenderTestURL("files/prerender/prerender_localstorage_write.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
}

// Checks that the favicon is properly loaded on prerender.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderFavicon) {
  PrerenderTestURL("files/prerender/prerender_favicon.html",
                   FINAL_STATUS_USED,
                   1);
  TestPrerenderContents* prerender_contents = GetPrerenderContents();
  ASSERT_TRUE(prerender_contents != NULL);
  ui_test_utils::WindowedNotificationObserver favicon_update_watcher(
      chrome::NOTIFICATION_FAVICON_UPDATED,
      content::Source<WebContents>(prerender_contents->prerender_contents()->
                          web_contents()));
  NavigateToDestURL();
  favicon_update_watcher.Wait();
}

// Checks that when a prerendered page is swapped in to a referring page, the
// unload handlers on the referring page are executed.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderUnload) {
  set_loader_path("files/prerender/prerender_loader_with_unload.html");
  PrerenderTestURL("files/prerender/prerender_page.html", FINAL_STATUS_USED, 1);
  string16 expected_title = ASCIIToUTF16("Unloaded");
  ui_test_utils::TitleWatcher title_watcher(
      current_browser()->GetSelectedWebContents(), expected_title);
  NavigateToDestURL();
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Checks that when the history is cleared, prerendering is cancelled and
// prerendering history is cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearHistory) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CACHE_OR_HISTORY_CLEARED,
                   1);

  // Post a task to clear the history, and run the message loop until it
  // destroys the prerender.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&ClearBrowsingData, current_browser(),
                 BrowsingDataRemover::REMOVE_HISTORY));
  ui_test_utils::RunMessageLoop();

  // Make sure prerender history was cleared.
  EXPECT_EQ(0, GetHistoryLength());
}

// Checks that when the cache is cleared, prerenders are cancelled but
// prerendering history is not cleared.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClearCache) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CACHE_OR_HISTORY_CLEARED,
                   1);

  // Post a task to clear the cache, and run the message loop until it
  // destroys the prerender.
  MessageLoop::current()->PostTask(FROM_HERE,
      base::Bind(&ClearBrowsingData, current_browser(),
                 BrowsingDataRemover::REMOVE_CACHE));
  ui_test_utils::RunMessageLoop();

  // Make sure prerender history was not cleared.  Not a vital behavior, but
  // used to compare with PrerenderClearHistory test.
  EXPECT_EQ(1, GetHistoryLength());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderCancelAll) {
  PrerenderTestURL("files/prerender/prerender_page.html",
                   FINAL_STATUS_CANCELLED,
                   1);
  // Post a task to cancel all the prerenders.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CancelAllPrerenders, GetPrerenderManager()));
  ui_test_utils::RunMessageLoop();
  EXPECT_TRUE(GetPrerenderContents() == NULL);
}

// Prerendering and history tests.
// The prerendered page is navigated to in several ways [navigate via
// omnibox, click on link, key-modified click to open in background tab, etc],
// followed by a navigation to another page from the prerendered page, followed
// by a back navigation.

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderNavigateClickGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  ClickToNextPageAfterPrerender(current_browser());
  GoBackToPrerender(current_browser());
}

// Disabled due to timeouts on commit queue.
// http://crbug.com/121130
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderNavigateNavigateGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  NavigateToNextPageAfterPrerender(current_browser());
  GoBackToPrerender(current_browser());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickClickGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  OpenDestURLViaClick();
  ClickToNextPageAfterPrerender(current_browser());
  GoBackToPrerender(current_browser());
}

// Disabled due to timeouts on commit queue.
// http://crbug.com/121130
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       DISABLED_PrerenderClickNavigateGoBack) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_USED,
                   1);
  OpenDestURLViaClick();
  NavigateToNextPageAfterPrerender(current_browser());
  GoBackToPrerender(current_browser());
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewWindow) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH,
                   1);
  OpenDestURLViaClickNewWindow();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewForegroundTab) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH,
                   1);
  OpenDestURLViaClickNewForegroundTab();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderClickNewBackgroundTab) {
  PrerenderTestURL("files/prerender/prerender_page_with_link.html",
                   FINAL_STATUS_SESSION_STORAGE_NAMESPACE_MISMATCH,
                   1);
  OpenDestURLViaClickNewBackgroundTab();
}

IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest,
                       NavigateToPrerenderedPageWhenDevToolsAttached) {
  DisableJavascriptCalls();
  WebContents* web_contents = current_browser()->GetSelectedWebContents();
  DevToolsAgentHost* agent = DevToolsAgentHostRegistry::GetDevToolsAgentHost(
      web_contents->GetRenderViewHost());
  DevToolsManager* manager = DevToolsManager::GetInstance();
  FakeDevToolsClientHost client_host;
  manager->RegisterDevToolsClientHostFor(agent, &client_host);
  const char* url = "files/prerender/prerender_page.html";
  PrerenderTestURL(url, FINAL_STATUS_DEVTOOLS_ATTACHED, 1);
  NavigateToURL(url);
  manager->ClientHostClosing(&client_host);
}

// Validate that the sessionStorage namespace remains the same when swapping
// in a prerendered page.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, PrerenderSessionStorage) {
  set_loader_path("files/prerender/prerender_loader_with_session_storage.html");
  PrerenderTestURL(GetCrossDomainTestUrl("files/prerender/prerender_page.html"),
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();
  GoBackToPageBeforePrerender(current_browser());
}

// Checks that the control group works.  A JS alert cannot be detected in the
// control group.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, ControlGroup) {
  RestorePrerenderMode restore_prerender_mode;
  PrerenderManager::SetMode(
      PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  PrerenderTestURL("files/prerender/prerender_alert_before_onload.html",
                   FINAL_STATUS_WOULD_HAVE_BEEN_USED, 0);
  NavigateToDestURL();
}

// Make sure that the MatchComplete dummy works in the normal case.  Once
// a prerender is cancelled because of a script, a dummy must be created to
// account for the MatchComplete case, and it must have a final status of
// FINAL_STATUS_WOULD_HAVE_BEEN_USED.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTest, MatchCompleteDummy) {
  std::deque<FinalStatus> expected_final_status_queue;
  expected_final_status_queue.push_back(FINAL_STATUS_JAVASCRIPT_ALERT);
  expected_final_status_queue.push_back(FINAL_STATUS_WOULD_HAVE_BEEN_USED);
  PrerenderTestURL("files/prerender/prerender_alert_before_onload.html",
                   expected_final_status_queue, 1);
  NavigateToDestURL();
}

class PrerenderBrowserTestWithNaCl : public PrerenderBrowserTest {
 public:
  PrerenderBrowserTestWithNaCl() {}
  virtual ~PrerenderBrowserTestWithNaCl() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    PrerenderBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableNaCl);
  }
};

// Check that NaCl plugins work when enabled, with prerendering.
IN_PROC_BROWSER_TEST_F(PrerenderBrowserTestWithNaCl,
                       PrerenderNaClPluginEnabled) {
  PrerenderTestURL("files/prerender/prerender_plugin_nacl_enabled.html",
                   FINAL_STATUS_USED,
                   1);
  NavigateToDestURL();

  // To avoid any chance of a race, we have to let the script send its response
  // asynchronously.
  WebContents* web_contents = browser()->GetSelectedWebContents();
  bool display_test_result = false;
  ASSERT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractBool(
      web_contents->GetRenderViewHost(), L"",
      L"DidDisplayReallyPass()",
      &display_test_result));
  ASSERT_TRUE(display_test_result);
}

}  // namespace prerender
