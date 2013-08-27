// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "base/command_line.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_result.h"
#include "chrome/browser/autocomplete/search_provider.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/google/google_url_tracker.h"
#include "chrome/browser/history/history_db_task.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_types.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/search/instant_ntp.h"
#include "chrome/browser/ui/search/instant_ntp_prerenderer.h"
#include "chrome/browser/ui/search/instant_tab.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/search_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/instant_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/thumbnail_score.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sessions/serialized_navigation_entry.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "grit/generated_resources.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"

using testing::HasSubstr;

namespace {

// Creates a bitmap of the specified color. Caller takes ownership.
gfx::Image CreateBitmap(SkColor color) {
  SkBitmap thumbnail;
  thumbnail.setConfig(SkBitmap::kARGB_8888_Config, 4, 4);
  thumbnail.allocPixels();
  thumbnail.eraseColor(color);
  return gfx::Image::CreateFrom1xBitmap(thumbnail);  // adds ref.
}

// Task used to make sure history has finished processing a request. Intended
// for use with BlockUntilHistoryProcessesPendingRequests.
class QuittingHistoryDBTask : public history::HistoryDBTask {
 public:
  QuittingHistoryDBTask() {}

  virtual bool RunOnDBThread(history::HistoryBackend* backend,
                             history::HistoryDatabase* db) OVERRIDE {
    return true;
  }

  virtual void DoneRunOnMainThread() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

 private:
  virtual ~QuittingHistoryDBTask() {}

  DISALLOW_COPY_AND_ASSIGN(QuittingHistoryDBTask);
};

class FakeNetworkChangeNotifier : public net::NetworkChangeNotifier {
 public:
  FakeNetworkChangeNotifier() : connection_type_(CONNECTION_NONE) {}

  virtual ConnectionType GetCurrentConnectionType() const OVERRIDE {
    return connection_type_;
  }

  void SetConnectionType(ConnectionType type) {
    connection_type_ = type;
    NotifyObserversOfNetworkChange(type);
    base::RunLoop().RunUntilIdle();
  }

  virtual ~FakeNetworkChangeNotifier() {}

 private:
  ConnectionType connection_type_;
  DISALLOW_COPY_AND_ASSIGN(FakeNetworkChangeNotifier);
};
}  // namespace

class InstantExtendedTest : public InProcessBrowserTest,
                            public InstantTestBase {
 public:
  InstantExtendedTest()
      : on_most_visited_change_calls_(0),
        most_visited_items_count_(0),
        first_most_visited_item_id_(0),
        on_native_suggestions_calls_(0),
        on_change_calls_(0),
        submit_count_(0),
        on_esc_key_press_event_calls_(0),
        on_focus_changed_calls_(0),
        is_focused_(false),
        on_toggle_voice_search_calls_(0) {
  }
 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url, false);
  }

  int64 GetHistogramCount(const char* name) {
    base::HistogramBase* histogram =
        base::StatisticsRecorder::FindHistogram(name);
    if (!histogram) {
      // If no histogram is found, it's possible that no values have been
      // recorded yet. Assume that the value is zero.
      return 0;
    }
    return histogram->SnapshotSamples()->TotalCount();
  }

  void SendDownArrow() {
    omnibox()->model()->OnUpOrDownKeyPressed(1);
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  void SendUpArrow() {
    omnibox()->model()->OnUpOrDownKeyPressed(-1);
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  void SendEscape() {
    omnibox()->model()->OnEscapeKeyPressed();
    // Wait for JavaScript to run the key handler by executing a blank script.
    EXPECT_TRUE(ExecuteScript(std::string()));
  }

  bool UpdateSearchState(content::WebContents* contents) WARN_UNUSED_RESULT {
    return GetIntFromJS(contents, "onMostVisitedChangedCalls",
                        &on_most_visited_change_calls_) &&
           GetIntFromJS(contents, "mostVisitedItemsCount",
                        &most_visited_items_count_) &&
           GetIntFromJS(contents, "firstMostVisitedItemId",
                        &first_most_visited_item_id_) &&
           GetIntFromJS(contents, "onNativeSuggestionsCalls",
                        &on_native_suggestions_calls_) &&
           GetIntFromJS(contents, "onChangeCalls",
                        &on_change_calls_) &&
           GetIntFromJS(contents, "submitCount",
                        &submit_count_) &&
           GetStringFromJS(contents, "apiHandle.value",
                           &query_value_) &&
           GetIntFromJS(contents, "onEscKeyPressedCalls",
                        &on_esc_key_press_event_calls_) &&
           GetIntFromJS(contents, "onFocusChangedCalls",
                       &on_focus_changed_calls_) &&
           GetBoolFromJS(contents, "isFocused",
                         &is_focused_) &&
           GetIntFromJS(contents, "onToggleVoiceSearchCalls",
                        &on_toggle_voice_search_calls_) &&
           GetStringFromJS(contents, "prefetchQuery", &prefetch_query_value_);

  }

  TemplateURL* GetDefaultSearchProviderTemplateURL() {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(browser()->profile());
    if (template_url_service)
      return template_url_service->GetDefaultSearchProvider();
    return NULL;
  }

  bool AddSearchToHistory(string16 term, int visit_count) {
    TemplateURL* template_url = GetDefaultSearchProviderTemplateURL();
    if (!template_url)
      return false;

    HistoryService* history = HistoryServiceFactory::GetForProfile(
        browser()->profile(), Profile::EXPLICIT_ACCESS);
    GURL search(template_url->url_ref().ReplaceSearchTerms(
        TemplateURLRef::SearchTermsArgs(term)));
    history->AddPageWithDetails(
        search, string16(), visit_count, visit_count,
        base::Time::Now(), false, history::SOURCE_BROWSED);
    history->SetKeywordSearchTermsForURL(
        search, template_url->id(), term);
    return true;
  }

  void BlockUntilHistoryProcessesPendingRequests() {
    HistoryService* history = HistoryServiceFactory::GetForProfile(
        browser()->profile(), Profile::EXPLICIT_ACCESS);
    DCHECK(history);
    DCHECK(base::MessageLoop::current());

    CancelableRequestConsumer consumer;
    history->ScheduleDBTask(new QuittingHistoryDBTask(), &consumer);
    base::MessageLoop::current()->Run();
  }

  int CountSearchProviderSuggestions() {
    return omnibox()->model()->autocomplete_controller()->search_provider()->
        matches().size();
  }

  int on_most_visited_change_calls_;
  int most_visited_items_count_;
  int first_most_visited_item_id_;
  int on_native_suggestions_calls_;
  int on_change_calls_;
  int submit_count_;
  int on_esc_key_press_event_calls_;
  std::string query_value_;
  int on_focus_changed_calls_;
  bool is_focused_;
  int on_toggle_voice_search_calls_;
  std::string prefetch_query_value_;
};

class InstantExtendedPrefetchTest : public InstantExtendedTest {
 public:
  InstantExtendedPrefetchTest()
      : factory_(new net::FakeURLFetcherFactory(
            net::URLFetcherImpl::factory())) {
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url, true);
    factory_->SetFakeResponse(GoogleURLTracker::kSearchDomainCheckURL,
                              ".google.com", true);
  }

  scoped_ptr<net::FakeURLFetcherFactory> factory_;

  DISALLOW_COPY_AND_ASSIGN(InstantExtendedPrefetchTest);
};

class InstantExtendedNetworkTest : public InstantExtendedTest {
 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    disable_for_test_.reset(new net::NetworkChangeNotifier::DisableForTest);
    fake_network_change_notifier_.reset(new FakeNetworkChangeNotifier);
    InstantExtendedTest::SetUpOnMainThread();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    InstantExtendedTest::CleanUpOnMainThread();
    fake_network_change_notifier_.reset();
    disable_for_test_.reset();
  }

  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    fake_network_change_notifier_->SetConnectionType(type);
  }

 private:
  scoped_ptr<net::NetworkChangeNotifier::DisableForTest> disable_for_test_;
  scoped_ptr<FakeNetworkChangeNotifier> fake_network_change_notifier_;
};

// Test class used to verify chrome-search: scheme and access policy from the
// Instant overlay.  This is a subclass of |ExtensionBrowserTest| because it
// loads a theme that provides a background image.
class InstantPolicyTest : public ExtensionBrowserTest, public InstantTestBase {
 public:
  InstantPolicyTest() {}

 protected:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    chrome::EnableInstantExtendedAPIForTesting();
    ASSERT_TRUE(https_test_server().Start());
    GURL instant_url = https_test_server().GetURL(
        "files/instant_extended.html?strk=1&");
    InstantTestBase::Init(instant_url, false);
  }

  void InstallThemeSource() {
    ThemeSource* theme = new ThemeSource(profile());
    content::URLDataSource::Add(profile(), theme);
  }

  void InstallThemeAndVerify(const std::string& theme_dir,
                             const std::string& theme_name) {
    const extensions::Extension* theme =
        ThemeServiceFactory::GetThemeForProfile(
            ExtensionBrowserTest::browser()->profile());
    // If there is already a theme installed, the current theme should be
    // disabled and the new one installed + enabled.
    int expected_change = theme ? 0 : 1;

    const base::FilePath theme_path = test_data_dir_.AppendASCII(theme_dir);
    ASSERT_TRUE(InstallExtensionWithUIAutoConfirm(
        theme_path, expected_change, ExtensionBrowserTest::browser()));
    const extensions::Extension* new_theme =
        ThemeServiceFactory::GetThemeForProfile(
            ExtensionBrowserTest::browser()->profile());
    ASSERT_NE(static_cast<extensions::Extension*>(NULL), new_theme);
    ASSERT_EQ(new_theme->name(), theme_name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InstantPolicyTest);
};

#if defined(HTML_INSTANT_EXTENDED_POPUP)

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, ExtendedModeIsOn) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  EXPECT_TRUE(chrome::IsInstantExtendedAPIEnabled());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NTPIsPreloaded) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
}
#endif  // HTML_INSTANT_EXTENDED_POPUP

IN_PROC_BROWSER_TEST_F(InstantExtendedNetworkTest, NTPReactsToNetworkChanges) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);

  // The setup first initializes the platform specific NetworkChangeNotifier.
  // The InstantExtendedNetworkTest replaces it with a fake, but by the time,
  // InstantNTPPrerenderer has already registered itself. So the
  // InstantNTPPrerenderer needs to register itself as NetworkChangeObserver
  // again.
  net::NetworkChangeNotifier::AddNetworkChangeObserver(
      instant_service->ntp_prerenderer());

  // The fake network change notifier will provide the network state to be
  // offline, so the ntp will be local.
  ASSERT_NE(static_cast<InstantNTP*>(NULL),
            instant_service->ntp_prerenderer()->ntp());
  EXPECT_TRUE(instant_service->ntp_prerenderer()->ntp()->IsLocal());

  // Change the connect state, and wait for the notifications to be run, and NTP
  // support to be determined.
  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_ETHERNET);
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Verify the network state is fine, and InstantNTPPrerenderer doesn't want
  // to switch to local NTP anymore.
  EXPECT_FALSE(net::NetworkChangeNotifier::IsOffline());
  EXPECT_FALSE(instant_service->ntp_prerenderer()->ShouldSwitchToLocalNTP());

  // Open new tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Verify new NTP is not local.
  EXPECT_TRUE(chrome::IsInstantNTP(active_tab));
  EXPECT_NE(instant_service->ntp_prerenderer()->GetLocalInstantURL(),
            active_tab->GetURL().spec());
  ASSERT_NE(static_cast<InstantNTP*>(NULL),
            instant_service->ntp_prerenderer()->ntp());
  EXPECT_FALSE(instant_service->ntp_prerenderer()->ntp()->IsLocal());

  SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Verify the network state is fine, and InstantNTPPrerenderer doesn't want
  // to switch to local NTP anymore.
  EXPECT_TRUE(net::NetworkChangeNotifier::IsOffline());
  EXPECT_TRUE(instant_service->ntp_prerenderer()->ShouldSwitchToLocalNTP());

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();

  // Verify new NTP is not local.
  EXPECT_TRUE(chrome::IsInstantNTP(active_tab));
  EXPECT_EQ(instant_service->ntp_prerenderer()->GetLocalInstantURL(),
            active_tab->GetURL().spec());
  ASSERT_NE(static_cast<InstantNTP*>(NULL),
            instant_service->ntp_prerenderer()->ntp());
  EXPECT_TRUE(instant_service->ntp_prerenderer()->ntp()->IsLocal());
}

#if defined(HTML_INSTANT_EXTENDED_POPUP)
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPIsUsedInSameTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(ntp_contents, active_tab);
  EXPECT_TRUE(chrome::IsInstantNTP(active_tab));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPForWrongProvider) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp_contents = instant()->ntp_->contents();
  EXPECT_TRUE(ntp_contents);
  GURL ntp_url = ntp_contents->GetURL();

  // Change providers.
  SetInstantURL("chrome://blank");

  // Open new tab. Preloaded NTP contents should have not been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(ntp_url, active_tab->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPRenderProcessGone) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  EXPECT_FALSE(instant()->ntp()->IsLocal());

  // NTP not reloaded after being killed.
  instant()->InstantPageRenderProcessGone(instant()->ntp()->contents());
  EXPECT_EQ(NULL, instant()->ntp());

  // Open new tab. Should use local NTP.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(instant()->GetLocalInstantURL(), active_tab->GetURL().spec());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, PreloadedNTPDoesntSupportInstant) {
  // Setup Instant.
  GURL instant_url = test_server()->GetURL("files/empty.html?strk=1");
  InstantTestBase::Init(instant_url);
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // NTP contents should have fallen back to the local page.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  EXPECT_TRUE(instant()->ntp()->IsLocal());

  // Open new tab. Should use local NTP.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(instant()->GetLocalInstantURL(), active_tab->GetURL().spec());
}

// Flaky, http://crbug.com/240852 .
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_OmniboxHasFocusOnNewTab) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Explicitly unfocus the omnibox.
  EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ui_test_utils::ClickOnView(browser(), VIEW_ID_TAB_CONTAINER);
  EXPECT_FALSE(omnibox()->model()->has_focus());

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Omnibox should have focus.
  EXPECT_TRUE(omnibox()->model()->has_focus());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxEmptyOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Omnibox should be empty.
  EXPECT_TRUE(omnibox()->GetText().empty());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoFaviconOnNewTabPage) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // No favicon should be shown.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  FaviconTabHelper* favicon_tab_helper =
      FaviconTabHelper::FromWebContents(active_tab);
  EXPECT_FALSE(favicon_tab_helper->ShouldDisplayFavicon());

  // Favicon should be shown off the NTP.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAboutURL));
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  favicon_tab_helper = FaviconTabHelper::FromWebContents(active_tab);
  EXPECT_TRUE(favicon_tab_helper->ShouldDisplayFavicon());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_MostVisited) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_SENT_MOST_VISITED_ITEMS,
      content::NotificationService::AllSources());
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Get a handle to the NTP and the current state of the JS.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  content::WebContents* ntp = instant()->ntp_->contents();
  EXPECT_TRUE(ntp);
  EXPECT_TRUE(UpdateSearchState(ntp));

  // Wait for most visited data to be ready, if necessary.
  if (on_most_visited_change_calls_ == 0) {
    observer.Wait();
    EXPECT_TRUE(UpdateSearchState(ntp));
  }

  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Make sure we have at least two Most Visited Items and save that number.
  // TODO(pedrosimonetti): For now, we're relying on the fact that the Top
  // Sites will have at lease two items in it. The correct approach would be
  // adding those items to the Top Sites manually before starting the test.
  EXPECT_GT(most_visited_items_count_, 1);
  int old_most_visited_items_count = most_visited_items_count_;

  // Delete the fist Most Visited Item.
  int rid = first_most_visited_item_id_;
  std::ostringstream stream;
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ");";
  EXPECT_TRUE(content::ExecuteScript(ntp, stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(ntp));

  // Make sure we have one less item in there.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count - 1);

  // Undo the deletion of the fist Most Visited Item.
  stream.str(std::string());
  stream << "newTabPageHandle.undoMostVisitedDeletion(" << rid << ");";
  EXPECT_TRUE(content::ExecuteScript(ntp, stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(ntp));

  // Make sure we have the same number of items as before.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count);

  // Delete the fist Most Visited Item.
  rid = first_most_visited_item_id_;
  stream.str(std::string());
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ");";
  EXPECT_TRUE(content::ExecuteScript(ntp, stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(ntp));

  // Delete the second Most Visited Item.
  rid = first_most_visited_item_id_;
  stream.str(std::string());
  stream << "newTabPageHandle.deleteMostVisitedItem(" << rid << ");";
  EXPECT_TRUE(content::ExecuteScript(ntp, stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(ntp));

  // Make sure we have two less items in there.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count - 2);

  // Delete the second Most Visited Item.
  stream.str(std::string());
  stream << "newTabPageHandle.undoAllMostVisitedDeletions();";
  EXPECT_TRUE(content::ExecuteScript(ntp, stream.str()));
  observer.Wait();

  // Update Most Visited state.
  EXPECT_TRUE(UpdateSearchState(ntp));

  // Make sure we have the same number of items as before.
  EXPECT_EQ(most_visited_items_count_, old_most_visited_items_count);
}

// TODO(dhollowa): Fix flakes.  http://crbug.com/179930.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_FaviconAccess) {
  // Create a favicon.
  history::TopSites* top_sites = browser()->profile()->GetTopSites();
  GURL url("http://www.google.com/foo.html");
  gfx::Image thumbnail(CreateBitmap(SK_ColorWHITE));
  ThumbnailScore high_score(0.0, true, true, base::Time::Now());
  EXPECT_TRUE(top_sites->SetPageThumbnail(url, thumbnail, high_score));

  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::RenderViewHost* rvh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  // Get the favicons.
  const std::string chrome_favicon_url(
      "chrome://favicon/largest/http://www.google.com/foo.html");
  const std::string search_favicon_url(
      "chrome-search://favicon/largest/http://www.google.com/foo.html");
  bool loaded = false;
  ASSERT_TRUE(LoadImage(rvh, chrome_favicon_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_favicon_url;
  ASSERT_TRUE(LoadImage(rvh, search_favicon_url, &loaded));
  EXPECT_TRUE(loaded) << search_favicon_url;
}

// Only implemented in Views and Mac currently: http://crbug.com/164723
#if defined(OS_WIN) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
#define MAYBE_HomeButtonAffectsMargin HomeButtonAffectsMargin
#else
#define MAYBE_HomeButtonAffectsMargin DISABLED_HomeButtonAffectsMargin
#endif

// Check that toggling the state of the home button changes the start-edge
// margin and width.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, MAYBE_HomeButtonAffectsMargin) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Get the current value of the start-edge margin and width.
  int start_margin;
  int width;
  content::WebContents* ntp = instant()->ntp_->contents();
  EXPECT_TRUE(GetIntFromJS(ntp, "chrome.searchBox.startMargin", &start_margin));
  EXPECT_TRUE(GetIntFromJS(ntp, "chrome.searchBox.width", &width));

  // Toggle the home button visibility pref.
  PrefService* profile_prefs = browser()->profile()->GetPrefs();
  bool show_home = profile_prefs->GetBoolean(prefs::kShowHomeButton);
  profile_prefs->SetBoolean(prefs::kShowHomeButton, !show_home);

  // Make sure the margin and width changed.
  int new_start_margin;
  int new_width;
  EXPECT_TRUE(GetIntFromJS(ntp, "chrome.searchBox.startMargin",
      &new_start_margin));
  EXPECT_TRUE(GetIntFromJS(ntp, "chrome.searchBox.width", &new_width));
  EXPECT_NE(start_margin, new_start_margin);
  EXPECT_NE(width, new_width);
  EXPECT_EQ(new_width - width, start_margin - new_start_margin);
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnNTP) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  const content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Instant-provided NTP should not have any bindings enabled.
  EXPECT_EQ(0, tab->GetRenderViewHost()->GetEnabledBindings());
}

// WebUIBindings should never be enabled on ANY Instant web contents.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoWebUIBindingsOnResults) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Type a query and press enter to get results.
  SetOmniboxText("query");
  // Commit the search by pressing Enter.
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_TRUE(instant()->model()->mode().is_default());
  const content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The commited Instant page should not have any bindings enabled.
  EXPECT_EQ(0, tab->GetRenderViewHost()->GetEnabledBindings());
}

// Test that the Bookmark provider is enabled, and returns results.
// TODO(sreeram): Convert this to a unit test.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_HasBookmarkProvider) {
  // No need to setup Instant.
  set_browser(browser());

  BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(bookmark_model);
  ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model);
  bookmark_utils::AddIfNotBookmarked(bookmark_model, GURL("http://angeline/"),
                                     ASCIIToUTF16("angeline"));

  SetOmniboxText("angeline");

  bool found_bookmark_match = false;

  const AutocompleteResult& result = omnibox()->model()->result();
  for (AutocompleteResult::const_iterator iter = result.begin();
       !found_bookmark_match && iter != result.end(); ++iter) {
    found_bookmark_match = iter->type == AutocompleteMatchType::BOOKMARK_TITLE;
  }

  EXPECT_TRUE(found_bookmark_match);
}

// Test that hitting Esc to clear the omnibox works. http://crbug.com/231744.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_EscapeClearsOmnibox) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Navigate to the Instant NTP, and wait for it to be recognized.
  content::WindowedNotificationObserver instant_tab_observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  instant_tab_observer.Wait();

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Type a query. Verify that the query is seen by the page.
  SetOmniboxText("mojo");
  std::string query;
  EXPECT_TRUE(GetStringFromJS(contents, "chrome.embeddedSearch.searchBox.value",
                              &query));
  EXPECT_EQ("mojo", query);

  EXPECT_TRUE(content::ExecuteScript(contents,
                                     "onChangeCalls = submitCount = 0;"));

  // Hit Escape, and verify that the page sees that the query is cleared.
  SendEscape();
  EXPECT_TRUE(GetStringFromJS(contents, "chrome.embeddedSearch.searchBox.value",
                              &query));
  EXPECT_EQ("", query);
  EXPECT_EQ("", GetOmniboxText());

  EXPECT_TRUE(UpdateSearchState(contents));
  EXPECT_LT(0, on_change_calls_);
  EXPECT_EQ(0, submit_count_);
  EXPECT_LT(0, on_esc_key_press_event_calls_);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, AcceptingURLSearchDoesNotNavigate) {
  // Get a committed Instant tab, which will be in the Instant process and thus
  // support chrome::GetSearchTerms().
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  SetOmniboxText("foo");
  EXPECT_EQ(ASCIIToUTF16("foo"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Set URL-like search terms for the instant tab.
  content::WebContents* instant_tab = instant()->instant_tab()->contents();
  content::NavigationEntry* visible_entry =
      instant_tab->GetController().GetVisibleEntry();
  visible_entry->SetExtraData(sessions::kSearchTermsKey,
                              ASCIIToUTF16("http://example.com"));
  SetOmniboxText("http://example.com");
  omnibox()->model()->SetInputInProgress(false);
  omnibox()->CloseOmniboxPopup();

  // Accept the omnibox input.
  EXPECT_FALSE(omnibox()->model()->user_input_in_progress());
  EXPECT_TRUE(
      browser()->toolbar_model()->WouldReplaceSearchURLWithSearchTerms());
  GURL instant_tab_url = instant_tab->GetURL();
  browser()->window()->GetLocationBar()->AcceptInput();
  EXPECT_EQ(instant_tab_url, instant_tab->GetURL());
}

// TODO(jered): Figure out why this test flakes and fix it.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       DISABLED_AcceptingJSSearchDoesNotRunJS) {
  // Get a committed Instant tab, which will be in the Instant process and thus
  // support chrome::GetSearchTerms().
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Do a search and commit it.
  SetOmniboxText("foo");
  EXPECT_EQ(ASCIIToUTF16("foo"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Set URL-like search terms for the instant tab.
  content::WebContents* instant_tab = instant()->instant_tab()->contents();
  content::NavigationEntry* visible_entry =
      instant_tab->GetController().GetVisibleEntry();
  const char kEvilJS[] = "javascript:document.title='evil';1;";
  visible_entry->SetExtraData(sessions::kSearchTermsKey, ASCIIToUTF16(kEvilJS));
  SetOmniboxText(kEvilJS);
  omnibox()->model()->SetInputInProgress(false);
  omnibox()->CloseOmniboxPopup();

  // Accept the omnibox input.
  EXPECT_FALSE(omnibox()->model()->user_input_in_progress());
  EXPECT_TRUE(
      browser()->toolbar_model()->WouldReplaceSearchURLWithSearchTerms());
  browser()->window()->GetLocationBar()->AcceptInput();
  // Force some Javascript to run in the renderer so the inline javascript:
  // would be forced to run if it's going to.
  EXPECT_TRUE(content::ExecuteScript(instant_tab, "1;"));
  EXPECT_NE(ASCIIToUTF16("evil"), instant_tab->GetTitle());
}

IN_PROC_BROWSER_TEST_F(
    InstantExtendedTest,
    DISABLED_ReloadSearchAfterBackReloadsCorrectQuery) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmnibox();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  // Search for [foo].
  SetOmniboxText("foo");
  EXPECT_EQ(ASCIIToUTF16("foo"), omnibox()->GetText());
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  // Search again for [bar].
  content::WebContents* instant_tab = instant()->instant_tab()->contents();
  EXPECT_TRUE(content::ExecuteScript(instant_tab,
                                     "suggestion = 'bart';"));
  SetOmniboxTextAndWaitForSuggestion("bar");

  // Accept the new query and wait for the page to navigate.
  content::WindowedNotificationObserver nav_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  browser()->window()->GetLocationBar()->AcceptInput();
  nav_observer.Wait();

  // Press back button and reload.
  content::WindowedNotificationObserver back_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  instant_tab->GetController().GoBack();
  back_observer.Wait();
  EXPECT_EQ("foo", GetOmniboxText());
  FocusOmnibox();
  content::WindowedNotificationObserver reload_observer(
        content::NOTIFICATION_NAV_ENTRY_COMMITTED,
        content::NotificationService::AllSources());
  browser()->window()->GetLocationBar()->AcceptInput();
  reload_observer.Wait();

  EXPECT_EQ("foo", GetOmniboxText());
}

class InstantExtendedFirstTabTest : public InProcessBrowserTest,
                                    public InstantTestBase {
 public:
  InstantExtendedFirstTabTest() {}
 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnableInstantExtendedAPI);
    command_line->AppendSwitch(switches::kDisableLocalFirstLoadNTP);
  }
};

// Flaky: http://crbug.com/238863
IN_PROC_BROWSER_TEST_F(
    InstantExtendedFirstTabTest, DISABLED_RedirectToLocalOnLoadFailure) {
  // Create a new window to test the first NTP load.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  const BrowserList* browser_list = BrowserList::GetInstance(
      chrome::GetActiveDesktop());
  ASSERT_EQ(2u, browser_list->size());
  set_browser(browser_list->get(1));

  FocusOmniboxAndWaitForInstantNTPSupport();

  // Also make sure our instant_tab_ is loaded.
  if (!instant()->instant_tab_) {
    content::WindowedNotificationObserver instant_tab_observer(
        chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
        content::NotificationService::AllSources());
    instant_tab_observer.Wait();
  }

  // NTP contents should be preloaded.
  ASSERT_NE(static_cast<InstantNTP*>(NULL), instant()->ntp());
  EXPECT_TRUE(instant()->ntp()->IsLocal());

  // Instant tab contents should be preloaded.
  ASSERT_NE(static_cast<InstantTab*>(NULL), instant()->instant_tab());
  EXPECT_TRUE(instant()->instant_tab()->IsLocal());
}

// Broken on mac: http://crbug.com/247448
#if defined(OS_MACOSX)
#define MAYBE_KeyboardTogglesVoiceSearch DISABLED_KeyboardTogglesVoiceSearch
#else
#define MAYBE_KeyboardTogglesVoiceSearch KeyboardTogglesVoiceSearch
#endif
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, MAYBE_KeyboardTogglesVoiceSearch) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Open new tab and test that toggle is fired.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_OEM_PERIOD,
                                              true, true, false, false));
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_toggle_voice_search_calls_);
}

// Test to verify that the omnibox search query is updated on browser
// back/forward button press events.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, UpdateSearchQueryOnNavigation) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox.
  FocusOmniboxAndWaitForInstantNTPSupport();
  SetOmniboxText("flowers");

  // Commit the search by pressing 'Enter'.
  PressEnterAndWaitForNavigation();
  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());

  // Typing in the new search query in omnibox.
  SetOmniboxText("cattles");
  // Commit the search by pressing 'Enter'.
  PressEnterAndWaitForNavigation();
  // 'Enter' commits the query as it was typed. This creates a navigation entry
  // in the history.
  EXPECT_EQ(ASCIIToUTF16("cattles"), omnibox()->GetText());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoBack();
  load_stop_observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());
  // Commit the search by pressing 'Enter'.
  FocusOmnibox();
  PressEnterAndWaitForNavigation();
  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());
}

#endif  // HTML_INSTANT_EXTENDED_POPUP

#if !defined(HTML_INSTANT_EXTENDED_POPUP)
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, SearchReusesInstantTab) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  SetOmniboxText("flowers");
  PressEnterAndWaitForNavigation();
  observer.Wait();

  // Just did a regular search.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_THAT(active_tab->GetURL().spec(), HasSubstr("q=flowers"));
  ASSERT_TRUE(UpdateSearchState(active_tab));
  ASSERT_EQ(0, submit_count_);

  SetOmniboxText("puppies");
  PressEnterAndWaitForNavigation();

  // Should have reused the tab and sent an onsubmit message.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_THAT(active_tab->GetURL().spec(), HasSubstr("q=puppies"));
  ASSERT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, submit_count_);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       SearchDoesntReuseInstantTabWithoutSupport) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Don't wait for the navigation to complete.
  SetOmniboxText("flowers");
  browser()->window()->GetLocationBar()->AcceptInput();

  SetOmniboxText("puppies");
  browser()->window()->GetLocationBar()->AcceptInput();

  // Should not have reused the tab.
  ASSERT_THAT(
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec(),
      HasSubstr("q=puppies"));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       TypedSearchURLDoesntReuseInstantTab) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer_1(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  SetOmniboxText("flowers");
  PressEnterAndWaitForNavigation();
  observer_1.Wait();

  // Just did a regular search.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_THAT(active_tab->GetURL().spec(), HasSubstr("q=flowers"));
  ASSERT_TRUE(UpdateSearchState(active_tab));
  ASSERT_EQ(0, submit_count_);

  // Typed in a search URL "by hand".
  content::WindowedNotificationObserver observer_2(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  SetOmniboxText(instant_url().spec() + "#q=puppies");
  PressEnterAndWaitForNavigation();
  observer_2.Wait();

  // Should not have reused the tab.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_THAT(active_tab->GetURL().spec(), HasSubstr("q=puppies"));
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest, OmniboxMarginSetForSearchURLs) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  SetOmniboxText("flowers");
  browser()->window()->GetLocationBar()->AcceptInput();
  observer.Wait();

  const std::string& url =
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec();
  // Make sure we actually used search_url, not instant_url.
  ASSERT_THAT(url, HasSubstr("&is_search"));
  EXPECT_THAT(url, HasSubstr("&es_sm="));
}

#endif  // if !defined(HTML_INSTANT_EXTENDED_POPUP)

// Test to verify that switching tabs should not dispatch onmostvisitedchanged
// events.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, NoMostVisitedChangedOnTabSwitch) {
  // Initialize Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure new tab received the onmostvisitedchanged event once.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);

  // Activate the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(0, false);

  // Switch back to new tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);

  // Confirm that new tab got no onmostvisitedchanged event.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);
}

IN_PROC_BROWSER_TEST_F(InstantPolicyTest, ThemeBackgroundAccess) {
  InstallThemeSource();
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // The "Instant" New Tab should have access to chrome-search: scheme but not
  // chrome: scheme.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  content::RenderViewHost* rvh =
      browser()->tab_strip_model()->GetActiveWebContents()->GetRenderViewHost();

  const std::string chrome_url("chrome://theme/IDR_THEME_NTP_BACKGROUND");
  const std::string search_url(
      "chrome-search://theme/IDR_THEME_NTP_BACKGROUND");
  bool loaded = false;
  ASSERT_TRUE(LoadImage(rvh, chrome_url, &loaded));
  EXPECT_FALSE(loaded) << chrome_url;
  ASSERT_TRUE(LoadImage(rvh, search_url, &loaded));
  EXPECT_TRUE(loaded) << search_url;
}

IN_PROC_BROWSER_TEST_F(InstantPolicyTest,
                       NoThemeBackgroundChangeEventOnTabSwitch) {
  InstallThemeSource();
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Install a theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  int on_theme_changed_calls = 0;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onThemeChangedCalls",
                           &on_theme_changed_calls));
  EXPECT_EQ(1, on_theme_changed_calls);

  // Activate the previous tab.
  browser()->tab_strip_model()->ActivateTabAt(0, false);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Switch back to new tab.
  browser()->tab_strip_model()->ActivateTabAt(1, false);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Confirm that new tab got no onthemechanged event while switching tabs.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  on_theme_changed_calls = 0;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onThemeChangedCalls",
                           &on_theme_changed_calls));
  EXPECT_EQ(1, on_theme_changed_calls);
}


// Flaky on Linux: http://crbug.com/265971
#if defined(OS_LINUX)
#define MAYBE_SendThemeBackgroundChangedEvent DISABLED_SendThemeBackgroundChangedEvent
#else
#define MAYBE_SendThemeBackgroundChangedEvent SendThemeBackgroundChangedEvent
#endif
IN_PROC_BROWSER_TEST_F(InstantPolicyTest,
                       MAYBE_SendThemeBackgroundChangedEvent) {
  InstallThemeSource();
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Install a theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme", "camo theme"));

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  // Make sure new tab received an onthemechanged event.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  int on_theme_changed_calls = 0;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onThemeChangedCalls",
                           &on_theme_changed_calls));
  EXPECT_EQ(1, on_theme_changed_calls);

  // Install a new theme.
  ASSERT_NO_FATAL_FAILURE(InstallThemeAndVerify("theme2", "snowflake theme"));

  // Confirm that new tab is notified about the theme changed event.
  on_theme_changed_calls = 0;
  EXPECT_TRUE(GetIntFromJS(active_tab, "onThemeChangedCalls",
                           &on_theme_changed_calls));
  EXPECT_EQ(2, on_theme_changed_calls);
}

// Flaky on Mac and Linux Tests bots.
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_UpdateSearchQueryOnBackNavigation DISABLED_UpdateSearchQueryOnBackNavigation
#else
#define MAYBE_UpdateSearchQueryOnBackNavigation UpdateSearchQueryOnBackNavigation
#endif
// Test to verify that the omnibox search query is updated on browser
// back button press event.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       MAYBE_UpdateSearchQueryOnBackNavigation) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  SetOmniboxText("flowers");
  // Commit the search by pressing 'Enter'.
  PressEnterAndWaitForNavigation();
  observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());

  // Typing in the new search query in omnibox.
  SetOmniboxText("cattles");
  // Commit the search by pressing 'Enter'.
  PressEnterAndWaitForNavigation();
  // 'Enter' commits the query as it was typed. This creates a navigation entry
  // in the history.
  EXPECT_EQ(ASCIIToUTF16("cattles"), omnibox()->GetText());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoBack();
  load_stop_observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());
  // Commit the search by pressing 'Enter'.
  FocusOmnibox();
  PressEnterAndWaitForNavigation();
  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());
}

// Flaky: crbug.com/253092.
// Test to verify that the omnibox search query is updated on browser
// forward button press events.
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       DISABLED_UpdateSearchQueryOnForwardNavigation) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));

  // Focus omnibox and confirm overlay isn't shown.
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Create an observer to wait for the instant tab to support Instant.
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());

  SetOmniboxText("flowers");
  // Commit the search by pressing 'Enter'.
  PressEnterAndWaitForNavigation();
  observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());

  // Typing in the new search query in omnibox.
  SetOmniboxText("cattles");
  // Commit the search by pressing 'Enter'.
  PressEnterAndWaitForNavigation();
  // 'Enter' commits the query as it was typed. This creates a navigation entry
  // in the history.
  EXPECT_EQ(ASCIIToUTF16("cattles"), omnibox()->GetText());

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoBack();
  load_stop_observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());

  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoForward());
  content::WindowedNotificationObserver load_stop_observer_2(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoForward();
  load_stop_observer_2.Wait();

  // Commit the search by pressing 'Enter'.
  FocusOmnibox();
  EXPECT_EQ(ASCIIToUTF16("cattles"), omnibox()->GetText());
  PressEnterAndWaitForNavigation();
  EXPECT_EQ(ASCIIToUTF16("cattles"), omnibox()->GetText());
}

// Flaky on all bots since re-enabled in r208032, crbug.com/253092
IN_PROC_BROWSER_TEST_F(InstantExtendedTest, DISABLED_NavigateBackToNTP) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Open a new tab page.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);
  EXPECT_EQ(2, browser()->tab_strip_model()->count());

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_INSTANT_TAB_SUPPORT_DETERMINED,
      content::NotificationService::AllSources());
  SetOmniboxText("flowers");
  PressEnterAndWaitForNavigation();
  observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());

  // Typing in the new search query in omnibox.
  // Commit the search by pressing 'Enter'.
  SetOmniboxText("cattles");
  PressEnterAndWaitForNavigation();

  // 'Enter' commits the query as it was typed. This creates a navigation entry
  // in the history.
  EXPECT_EQ(ASCIIToUTF16("cattles"), omnibox()->GetText());

  // Navigate back to "flowers" search result page.
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoBack();
  load_stop_observer.Wait();

  EXPECT_EQ(ASCIIToUTF16("flowers"), omnibox()->GetText());

  // Navigate back to NTP.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  content::WindowedNotificationObserver load_stop_observer_2(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &active_tab->GetController()));
  active_tab->GetController().GoBack();
  load_stop_observer_2.Wait();

  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome::IsInstantNTP(active_tab));
}

// Flaky: crbug.com/267119
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       DISABLED_DispatchMVChangeEventWhileNavigatingBackToNTP) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Open new tab. Preloaded NTP contents should have been used.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);

  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  // Set the text and press enter to navigate from NTP.
  SetOmniboxText("Pen");
  PressEnterAndWaitForNavigation();
  EXPECT_EQ(ASCIIToUTF16("Pen"), omnibox()->GetText());
  observer.Wait();

  // Navigate back to NTP.
  content::WindowedNotificationObserver back_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(active_tab->GetController().CanGoBack());
  active_tab->GetController().GoBack();
  back_observer.Wait();

  // Verify that onmostvisitedchange event is dispatched when we navigate from
  // SRP to NTP.
  active_tab = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(UpdateSearchState(active_tab));
  EXPECT_EQ(1, on_most_visited_change_calls_);
}

// Flaky: crbug.com/267096
IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       DISABLED_OnDefaultSearchProviderChanged) {
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(static_cast<InstantService*>(NULL), instant_service);

  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();
  EXPECT_EQ(1, instant_service->GetInstantProcessCount());

  // Navigating to the NTP should use the Instant render process.
  content::WindowedNotificationObserver new_tab_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  new_tab_observer.Wait();

  content::WebContents* ntp_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(chrome::IsInstantNTP(ntp_contents));
  EXPECT_TRUE(instant_service->IsInstantProcess(
      ntp_contents->GetRenderProcessHost()->GetID()));
  GURL ntp_url = ntp_contents->GetURL();

  AddBlankTabAndShow(browser());
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(chrome::IsInstantNTP(active_tab));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      active_tab->GetRenderProcessHost()->GetID()));

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("t");
  data.SetURL("http://defaultturl/q={searchTerms}");
  data.suggestions_url = "http://defaultturl2/q={searchTerms}";
  data.instant_url = "http://does/not/exist";
  data.alternate_urls.push_back(data.instant_url + "#q={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  service->Add(template_url);  // Takes ownership of |template_url|.

  // Change the default search provider.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &ntp_contents->GetController()));
  service->SetDefaultSearchProvider(template_url);
  observer.Wait();

  // |ntp_contents| should not use the Instant render process.
  EXPECT_FALSE(chrome::IsInstantNTP(ntp_contents));
  EXPECT_FALSE(instant_service->IsInstantProcess(
      ntp_contents->GetRenderProcessHost()->GetID()));
  // Make sure the URL remains the same.
  EXPECT_EQ(ntp_url, ntp_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedTest,
                       ReloadLocalNTPOnSearchProviderChange) {
  // Setup Instant.
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  // Navigate to Local NTP.
  content::WindowedNotificationObserver new_tab_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeSearchLocalNtpUrl),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  new_tab_observer.Wait();

  content::WebContents* ntp_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL ntp_url = ntp_contents->GetURL();

  TemplateURLData data;
  data.short_name = ASCIIToUTF16("t");
  data.SetURL("http://defaultturl/q={searchTerms}");
  data.suggestions_url = "http://defaultturl2/q={searchTerms}";
  data.instant_url = "http://does/not/exist";
  data.alternate_urls.push_back(data.instant_url + "#q={searchTerms}");
  data.search_terms_replacement_key = "strk";

  TemplateURL* template_url = new TemplateURL(browser()->profile(), data);
  TemplateURLService* service =
      TemplateURLServiceFactory::GetForProfile(browser()->profile());
  ui_test_utils::WaitForTemplateURLServiceToLoad(service);
  service->Add(template_url);  // Takes ownership of |template_url|.

  // Change the default search provider. This will reload the local NTP and the
  // page URL will remain the same.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<content::NavigationController>(
          &ntp_contents->GetController()));
  service->SetDefaultSearchProvider(template_url);
  observer.Wait();

  // Make sure the URL remains the same.
  EXPECT_EQ(ntp_url, ntp_contents->GetURL());
}

IN_PROC_BROWSER_TEST_F(InstantExtendedPrefetchTest, SetPrefetchQuery) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  content::WindowedNotificationObserver new_tab_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  new_tab_observer.Wait();

  omnibox()->model()->autocomplete_controller()->search_provider()->
      kMinimumTimeBetweenSuggestQueriesMs = 0;

  // Set the fake response for suggest request. Response has prefetch details.
  // Ensure that the page received the prefetch query.
  factory_->SetFakeResponse(
      instant_url().spec() + "#q=pupp",
      "[\"pupp\",[\"puppy\", \"puppies\"],[],[],"
      "{\"google:clientdata\":{\"phi\": 0},"
          "\"google:suggesttype\":[\"QUERY\", \"QUERY\"],"
          "\"google:suggestrelevance\":[1400, 9]}]",
      true);

  SetOmniboxText("pupp");
  while (!omnibox()->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver ready_observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::Source<AutocompleteController>(
            omnibox()->model()->autocomplete_controller()));
    ready_observer.Wait();
  }

  ASSERT_EQ(3, CountSearchProviderSuggestions());
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(UpdateSearchState(active_tab));
  ASSERT_TRUE(SearchProvider::ShouldPrefetch(*(
      omnibox()->model()->result().default_match())));
  ASSERT_EQ("puppy", prefetch_query_value_);
}

IN_PROC_BROWSER_TEST_F(InstantExtendedPrefetchTest, ClearPrefetchedResults) {
  ASSERT_NO_FATAL_FAILURE(SetupInstant(browser()));
  FocusOmniboxAndWaitForInstantNTPSupport();

  content::WindowedNotificationObserver new_tab_observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(),
      GURL(chrome::kChromeUINewTabURL),
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  new_tab_observer.Wait();

  omnibox()->model()->autocomplete_controller()->search_provider()->
      kMinimumTimeBetweenSuggestQueriesMs = 0;

  // Set the fake response for suggest request. Response has no prefetch
  // details. Ensure that the page received a blank query to clear the
  // prefetched results.
  factory_->SetFakeResponse(
      instant_url().spec() + "#q=dogs",
      "[\"dogs\",[\"https://dogs.com\"],[],[],"
          "{\"google:suggesttype\":[\"NAVIGATION\"],"
          "\"google:suggestrelevance\":[2]}]",
      true);

  SetOmniboxText("dogs");
  while (!omnibox()->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver ready_observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::Source<AutocompleteController>(
            omnibox()->model()->autocomplete_controller()));
    ready_observer.Wait();
  }

  ASSERT_EQ(2, CountSearchProviderSuggestions());
  ASSERT_FALSE(SearchProvider::ShouldPrefetch(*(
      omnibox()->model()->result().default_match())));
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(UpdateSearchState(active_tab));
  ASSERT_EQ("", prefetch_query_value_);
}
