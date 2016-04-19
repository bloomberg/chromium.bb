// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_usages_state.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/geoposition.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

std::string GetErrorCodePermissionDenied() {
  return base::IntToString(content::Geoposition::ERROR_CODE_PERMISSION_DENIED);
}

std::string RunScript(content::RenderFrameHost* render_frame_host,
                      const std::string& script) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(render_frame_host, script,
                                                     &result));
  return result;
}

// IFrameLoader ---------------------------------------------------------------

// Used to block until an iframe is loaded via a javascript call.
// Note: NavigateToURLBlockUntilNavigationsComplete doesn't seem to work for
// multiple embedded iframes, as notifications seem to be 'batched'. Instead, we
// load and wait one single frame here by calling a javascript function.
class IFrameLoader : public content::NotificationObserver {
 public:
  IFrameLoader(Browser* browser, int iframe_id, const GURL& url);
  ~IFrameLoader() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  const GURL& iframe_url() const { return iframe_url_; }

 private:
  content::NotificationRegistrar registrar_;

  // If true the navigation has completed.
  bool navigation_completed_;

  // If true the javascript call has completed.
  bool javascript_completed_;

  std::string javascript_response_;

  // The URL for the iframe we just loaded.
  GURL iframe_url_;

  DISALLOW_COPY_AND_ASSIGN(IFrameLoader);
};

IFrameLoader::IFrameLoader(Browser* browser, int iframe_id, const GURL& url)
    : navigation_completed_(false),
      javascript_completed_(false) {
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  content::NavigationController* controller = &web_contents->GetController();
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<content::NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
  std::string script(base::StringPrintf(
      "window.domAutomationController.setAutomationId(0);"
      "window.domAutomationController.send(addIFrame(%d, \"%s\"));",
      iframe_id, url.spec().c_str()));
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script));
  content::RunMessageLoop();

  EXPECT_EQ(base::StringPrintf("\"%d\"", iframe_id), javascript_response_);
  registrar_.RemoveAll();
  // Now that we loaded the iframe, let's fetch its src.
  script = base::StringPrintf(
      "window.domAutomationController.send(getIFrameSrc(%d))", iframe_id);
  iframe_url_ = GURL(RunScript(web_contents->GetMainFrame(), script));
}

IFrameLoader::~IFrameLoader() {
}

void IFrameLoader::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    navigation_completed_ = true;
  } else if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
    content::Details<std::string> dom_op_result(details);
    javascript_response_ = *dom_op_result.ptr();
    javascript_completed_ = true;
  }
  if (javascript_completed_ && navigation_completed_)
    base::MessageLoopForUI::current()->QuitWhenIdle();
}

// PermissionBubbleObserver ---------------------------------------------------

// Used to observe the creation of a single permission bubble without
// responding.
class PermissionBubbleObserver : public PermissionBubbleManager::Observer {
 public:
  explicit PermissionBubbleObserver(content::WebContents* web_contents)
      : bubble_manager_(PermissionBubbleManager::FromWebContents(web_contents)),
        request_shown_(false),
        message_loop_runner_(new content::MessageLoopRunner) {
    bubble_manager_->AddObserver(this);
  }
  ~PermissionBubbleObserver() override {
    // Safe to remove twice if it happens.
    bubble_manager_->RemoveObserver(this);
  }

  void Wait() { message_loop_runner_->Run(); }

  bool request_shown() { return request_shown_; }

 private:
  // PermissionBubbleManager::Observer
  void OnBubbleAdded() override {
    request_shown_ = true;
    bubble_manager_->RemoveObserver(this);
    message_loop_runner_->Quit();
  }

  PermissionBubbleManager* bubble_manager_;
  bool request_shown_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleObserver);
};

}  // namespace


// GeolocationBrowserTest -----------------------------------------------------

// This is a browser test for Geolocation.
// It exercises various integration points from javascript <-> browser:
// 1. The user is prompted when a position is requested from an unauthorized
//    origin.
// 2. Denying the request triggers the correct error callback.
// 3. Granting permission does not trigger an error, and allows a position to
//    be passed to javascript.
// 4. Permissions persisted in disk are respected.
// 5. Incognito profiles don't persist permissions on disk, but they do inherit
//    them from their regular parent profile.
class GeolocationBrowserTest : public InProcessBrowserTest {
 public:
  enum InitializationOptions {
    // The default profile and browser window will be used.
    INITIALIZATION_DEFAULT,

    // An incognito profile and browser window will be used.
    INITIALIZATION_OFFTHERECORD,

    // A new tab will be created using the default profile and browser window.
    INITIALIZATION_NEWTAB,
  };

  GeolocationBrowserTest();
  ~GeolocationBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownInProcessBrowserTestFixture() override;

  Browser* current_browser() { return current_browser_; }
  void set_html_for_tests(const std::string& html_for_tests) {
    html_for_tests_ = html_for_tests;
  }
  const GURL& current_url() const { return current_url_; }
  const GURL& iframe_url(size_t i) const { return iframe_urls_[i]; }
  double fake_latitude() const { return fake_latitude_; }
  double fake_longitude() const { return fake_longitude_; }

  // Initializes the test server and navigates to the initial url.
  void Initialize(InitializationOptions options);

  // Loads two iframes with different origins: http://127.0.0.1 and
  // http://localhost.
  void LoadIFrames();

  // Specifies which frame to use for executing JavaScript.
  void SetFrameForScriptExecution(const std::string& frame_name);

  // Gets the HostContentSettingsMap for the current profile.
  HostContentSettingsMap* GetHostContentSettingsMap();

  // Calls watchPosition in JavaScript and accepts or denies the resulting
  // permission bubble. Returns |true| if the expected behavior happened.
  bool WatchPositionAndGrantPermission() WARN_UNUSED_RESULT;
  bool WatchPositionAndDenyPermission() WARN_UNUSED_RESULT;

  // Calls watchPosition in JavaScript and observes whether the permission
  // bubble is shown without interacting with it. Callers should set
  // |bubble_should_display| to |true| if they expect a bubble to display.
  void WatchPositionAndObservePermissionBubble(bool bubble_should_display);

  // Checks that no errors have been received in JavaScript, and checks that the
  // position most recently received matches |latitude| and |longitude|.
  void ExpectPosition(double latitude, double longitude);

  // Executes |function| in |render_frame_host| and checks that the return value
  // matches |expected|.
  void ExpectValueFromScriptForFrame(
      const std::string& expected,
      const std::string& function,
      content::RenderFrameHost* render_frame_host);

  // Executes |function| and checks that the return value matches |expected|.
  void ExpectValueFromScript(const std::string& expected,
                             const std::string& function);

  // Sets a new (second) position and runs all callbacks currently registered
  // with the Geolocation system. Returns |true| if the new position is updated
  // successfully in JavaScript.
  bool SetPositionAndWaitUntilUpdated(double latitude, double longitude);

  // Convenience method to look up the number of queued permission bubbles.
  int GetBubbleQueueSize(PermissionBubbleManager* manager);

 private:
  // Calls watchPosition() in JavaScript and accepts or denies the resulting
  // permission bubble. Returns the JavaScript response.
  std::string WatchPositionAndRespondToPermissionBubble(
      PermissionBubbleManager::AutoResponseType bubble_response);

  // The current Browser as set in Initialize. May be for an incognito profile.
  Browser* current_browser_ = nullptr;

  // The path element of a URL referencing the html content for this test.
  std::string html_for_tests_ = "/geolocation/simple.html";

  // The frame where the JavaScript calls will run.
  content::RenderFrameHost* render_frame_host_ = nullptr;

  // The current url for the top level page.
  GURL current_url_;

  // The urls for the iframes loaded by LoadIFrames.
  std::vector<GURL> iframe_urls_;

  // The values used for the position override.
  double fake_latitude_ = 1.23;
  double fake_longitude_ = 4.56;

  DISALLOW_COPY_AND_ASSIGN(GeolocationBrowserTest);
};

GeolocationBrowserTest::GeolocationBrowserTest() {
}

GeolocationBrowserTest::~GeolocationBrowserTest() {
}

void GeolocationBrowserTest::SetUpOnMainThread() {
  ui_test_utils::OverrideGeolocation(fake_latitude_, fake_longitude_);
}

void GeolocationBrowserTest::TearDownInProcessBrowserTestFixture() {
  LOG(WARNING) << "TearDownInProcessBrowserTestFixture. Test Finished.";
}

void GeolocationBrowserTest::Initialize(InitializationOptions options) {
  if (!embedded_test_server()->Started() && !embedded_test_server()->Start()) {
    ADD_FAILURE() << "Test server failed to start.";
    return;
  }

  current_url_ = embedded_test_server()->GetURL(html_for_tests_);
  if (options == INITIALIZATION_OFFTHERECORD) {
    current_browser_ = OpenURLOffTheRecord(browser()->profile(), current_url_);
  } else {
    current_browser_ = browser();
    if (options == INITIALIZATION_NEWTAB)
      chrome::NewTab(current_browser_);
  }
  ASSERT_TRUE(current_browser_);
  if (options != INITIALIZATION_OFFTHERECORD)
    ui_test_utils::NavigateToURL(current_browser_, current_url_);

  // By default the main frame is used for JavaScript execution.
  SetFrameForScriptExecution("");
}

void GeolocationBrowserTest::LoadIFrames() {
  int number_iframes = 2;
  iframe_urls_.resize(number_iframes);
  for (int i = 0; i < number_iframes; ++i) {
    IFrameLoader loader(current_browser_, i, GURL());
    iframe_urls_[i] = loader.iframe_url();
  }
}

void GeolocationBrowserTest::SetFrameForScriptExecution(
    const std::string& frame_name) {
  content::WebContents* web_contents =
      current_browser_->tab_strip_model()->GetActiveWebContents();
  render_frame_host_ = nullptr;

  if (frame_name.empty()) {
    render_frame_host_ = web_contents->GetMainFrame();
  } else {
    render_frame_host_ = content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, frame_name));
  }
  DCHECK(render_frame_host_);
}

HostContentSettingsMap* GeolocationBrowserTest::GetHostContentSettingsMap() {
  return HostContentSettingsMapFactory::GetForProfile(
      current_browser()->profile());
}

bool GeolocationBrowserTest::WatchPositionAndGrantPermission() {
  std::string result = WatchPositionAndRespondToPermissionBubble(
      PermissionBubbleManager::ACCEPT_ALL);
  return "request-callback-success" == result;
}

bool GeolocationBrowserTest::WatchPositionAndDenyPermission() {
  std::string result = WatchPositionAndRespondToPermissionBubble(
      PermissionBubbleManager::DENY_ALL);
  return "request-callback-error" == result;
}

std::string GeolocationBrowserTest::WatchPositionAndRespondToPermissionBubble(
    PermissionBubbleManager::AutoResponseType bubble_response) {
  PermissionBubbleManager::FromWebContents(
      current_browser_->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(bubble_response);
  return RunScript(render_frame_host_, "geoStartWithAsyncResponse()");
}

void GeolocationBrowserTest::WatchPositionAndObservePermissionBubble(
    bool bubble_should_display) {
  PermissionBubbleObserver observer(
      current_browser_->tab_strip_model()->GetActiveWebContents());
  if (bubble_should_display) {
    // Control will return as soon as the API call is made, and then the
    // observer will wait for the bubble to display.
    RunScript(render_frame_host_, "geoStartWithSyncResponse()");
    observer.Wait();
  } else {
    // Control will return once one of the callbacks fires.
    RunScript(render_frame_host_, "geoStartWithAsyncResponse()");
  }
  EXPECT_EQ(bubble_should_display, observer.request_shown());
}

void GeolocationBrowserTest::ExpectPosition(double latitude, double longitude) {
  // Checks we have no error.
  ExpectValueFromScript("0", "geoGetLastError()");
  ExpectValueFromScript(base::DoubleToString(latitude),
                        "geoGetLastPositionLatitude()");
  ExpectValueFromScript(base::DoubleToString(longitude),
                        "geoGetLastPositionLongitude()");
}

void GeolocationBrowserTest::ExpectValueFromScriptForFrame(
    const std::string& expected,
    const std::string& function,
    content::RenderFrameHost* render_frame_host) {
  std::string script(base::StringPrintf(
      "window.domAutomationController.send(%s)", function.c_str()));
  EXPECT_EQ(expected, RunScript(render_frame_host, script));
}

void GeolocationBrowserTest::ExpectValueFromScript(
    const std::string& expected,
    const std::string& function) {
  ExpectValueFromScriptForFrame(expected, function, render_frame_host_);
}

bool GeolocationBrowserTest::SetPositionAndWaitUntilUpdated(double latitude,
                                                            double longitude) {
  fake_latitude_ = latitude;
  fake_longitude_ = longitude;
  ui_test_utils::OverrideGeolocation(latitude, longitude);

  // Now wait until the new position gets to the script.
  // Control will return (a) if the update has already been received, or (b)
  // when the update is received. This will hang if the position is never
  // updated. Currently this expects the position to be updated once; if your
  // test updates it repeatedly, |position_updated| (JS) needs to change to an
  // int to count how often it's been updated.
  std::string result =
      RunScript(render_frame_host_, "checkIfGeopositionUpdated()");
  return result == "geoposition-updated";
}

int GeolocationBrowserTest::GetBubbleQueueSize(
    PermissionBubbleManager* manager) {
  return static_cast<int>(manager->requests_.size());
}

// Tests ----------------------------------------------------------------------

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_DisplaysPrompt DISABLED_DisplaysPrompt
#else
#define MAYBE_DisplaysPrompt DisplaysPrompt
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_DisplaysPrompt) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_Geoposition DISABLED_Geoposition
#else
#define MAYBE_Geoposition Geoposition
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_Geoposition) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, ErrorOnPermissionDenied) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  EXPECT_TRUE(WatchPositionAndDenyPermission());
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_NoPromptForSecondTab DISABLED_NoPromptForSecondTab
#else
#define MAYBE_NoPromptForSecondTab NoPromptForSecondTab
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoPromptForSecondTab) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());

  // Checks bubble is not needed in a second tab.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_NEWTAB));
  WatchPositionAndObservePermissionBubble(false);
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForDeniedOrigin) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      current_url(), current_url(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(), CONTENT_SETTING_BLOCK);

  // Check that the bubble wasn't shown but we get an error for this origin.
  WatchPositionAndObservePermissionBubble(false);
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");

  // Checks prompt will not be created a second tab.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_NEWTAB));
  WatchPositionAndObservePermissionBubble(false);
  ExpectValueFromScript(GetErrorCodePermissionDenied(), "geoGetLastError()");
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_NoPromptForAllowedOrigin DISABLED_NoPromptForAllowedOrigin
#else
#define MAYBE_NoPromptForAllowedOrigin NoPromptForAllowedOrigin
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoPromptForAllowedOrigin) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      current_url(), current_url(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(), CONTENT_SETTING_ALLOW);
  // The bubble is not shown, there is no error, and the position gets to the
  // script.
  WatchPositionAndObservePermissionBubble(false);
  ExpectPosition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForOffTheRecord) {
  // For a regular profile the user is prompted, and when granted the position
  // gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // The permission is persisted for the regular profile, and inherited by its
  // incognito profile. Go incognito, and check that the user is not prompted
  // again and the position gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_OFFTHERECORD));
  WatchPositionAndObservePermissionBubble(false);
  ExpectPosition(fake_latitude(), fake_longitude());
}

// http://crbug.com/523387
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       DISABLED_NoLeakFromOffTheRecord) {
  // The user is prompted in a fresh incognito profile, and when granted the
  // position gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_OFFTHERECORD));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // The regular profile knows nothing of what happened in incognito. It is
  // prompted and when granted the position gets to the script.
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_IFramesWithFreshPosition DISABLED_IFramesWithFreshPosition
#else
#define MAYBE_IFramesWithFreshPosition IFramesWithFreshPosition
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_IFramesWithFreshPosition) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  // Grant permission in the first frame, the position gets to the script.
  SetFrameForScriptExecution("iframe_0");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // In a second iframe from a different origin with a cached position the user
  // is prompted.
  SetFrameForScriptExecution("iframe_1");
  WatchPositionAndObservePermissionBubble(true);

  // Back to the first frame, enable navigation and refresh geoposition.
  SetFrameForScriptExecution("iframe_0");
  double fresh_position_latitude = 3.17;
  double fresh_position_longitude = 4.23;
  ASSERT_TRUE(SetPositionAndWaitUntilUpdated(fresh_position_latitude,
                                             fresh_position_longitude));
  ExpectPosition(fresh_position_latitude, fresh_position_longitude);

  // When permission is granted to the second iframe the fresh position gets to
  // the script.
  SetFrameForScriptExecution("iframe_1");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fresh_position_latitude, fresh_position_longitude);
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_IFramesWithCachedPosition DISABLED_IFramesWithCachedPosition
#else
#define MAYBE_IFramesWithCachedPosition IFramesWithCachedPosition
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       MAYBE_IFramesWithCachedPosition) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  // Grant permission in the first frame, the position gets to the script.
  SetFrameForScriptExecution("iframe_0");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Refresh the position, but let's not yet create the watch on the second
  // frame so that it'll fetch from cache.
  double cached_position_latitude = 5.67;
  double cached_position_lognitude = 8.09;
  ASSERT_TRUE(SetPositionAndWaitUntilUpdated(cached_position_latitude,
                                             cached_position_lognitude));
  ExpectPosition(cached_position_latitude, cached_position_lognitude);

  // Now check the second frame gets cached values as well.
  SetFrameForScriptExecution("iframe_1");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(cached_position_latitude, cached_position_lognitude);
}

// http://crbug.com/523387
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       DISABLED_CancelPermissionForFrame) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  SetFrameForScriptExecution("iframe_0");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Test second iframe from a different origin with a cached position will
  // create the prompt.
  SetFrameForScriptExecution("iframe_1");
  WatchPositionAndObservePermissionBubble(true);

  // Navigate the iframe, and ensure the prompt is gone.
  content::WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  IFrameLoader change_iframe_1(current_browser(), 1, current_url());
  int num_bubbles_after_cancel = GetBubbleQueueSize(
      PermissionBubbleManager::FromWebContents(web_contents));
  EXPECT_EQ(0, num_bubbles_after_cancel);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, InvalidUrlRequest) {
  // Tests that an invalid URL (e.g. from a popup window) is rejected
  // correctly. Also acts as a regression test for http://crbug.com/40478
  set_html_for_tests("/geolocation/invalid_request_url.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));

  content::WebContents* original_tab =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  ExpectValueFromScript(GetErrorCodePermissionDenied(),
                        "requestGeolocationFromInvalidUrl()");
  ExpectValueFromScriptForFrame("1", "isAlive()", original_tab->GetMainFrame());
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_NoPromptBeforeStart DISABLED_NoPromptBeforeStart
#else
#define MAYBE_NoPromptBeforeStart NoPromptBeforeStart
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoPromptBeforeStart) {
  // See http://crbug.com/42789
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  // In the second iframe, access the navigator.geolocation object, but don't
  // call any methods yet so it won't request permission yet.
  SetFrameForScriptExecution("iframe_1");
  ExpectValueFromScript("object", "geoAccessNavigatorGeolocation()");

  // In the first iframe, call watchPosition, grant permission, and verify that
  // the position gets to the script.
  SetFrameForScriptExecution("iframe_0");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // Back to the second frame. The user is prompted now (it has a different
  // origin). When permission is granted the position gets to the script.
  SetFrameForScriptExecution("iframe_1");
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_TwoWatchesInOneFrame DISABLED_TwoWatchesInOneFrame
#else
#define MAYBE_TwoWatchesInOneFrame TwoWatchesInOneFrame
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_TwoWatchesInOneFrame) {
  set_html_for_tests("/geolocation/two_watches.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));

  // Tell the script what to expect as the final coordinates.
  double final_position_latitude = 3.17;
  double final_position_longitude = 4.23;
  std::string script =
      base::StringPrintf("geoSetFinalPosition(%f, %f)", final_position_latitude,
                         final_position_longitude);
  ExpectValueFromScript("ok", script);

  // Request permission and set two watches for the initial success callback.
  ASSERT_TRUE(WatchPositionAndGrantPermission());
  ExpectPosition(fake_latitude(), fake_longitude());

  // The second watch will now have cancelled. Ensure an update still makes
  // its way through to the first watcher.
  ASSERT_TRUE(SetPositionAndWaitUntilUpdated(final_position_latitude,
                                             final_position_longitude));
  ExpectPosition(final_position_latitude, final_position_longitude);
}

// TODO(felt): Disabled because the second permission request hangs.
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DISABLED_PendingChildFrames) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  LoadIFrames();

  SetFrameForScriptExecution("iframe_0");
  WatchPositionAndObservePermissionBubble(true);

  SetFrameForScriptExecution("iframe_1");
  WatchPositionAndObservePermissionBubble(true);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TabDestroyed) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  WatchPositionAndObservePermissionBubble(true);

  // TODO(mvanouwerkerk): Can't close a window you did not open. Maybe this was
  // valid when the test was written, but now it just prints "Scripts may close
  // only the windows that were opened by it."
  std::string script = "window.domAutomationController.send(window.close())";
  ASSERT_TRUE(content::ExecuteScript(
      current_browser()->tab_strip_model()->GetActiveWebContents(), script));
}

#if defined(OS_LINUX)
// http://crbug.com/527437
#define MAYBE_LastUsageUpdated DISABLED_LastUsageUpdated
#else
#define MAYBE_LastUsageUpdated LastUsageUpdated
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_LastUsageUpdated) {
  ASSERT_NO_FATAL_FAILURE(Initialize(INITIALIZATION_DEFAULT));
  base::SimpleTestClock* clock_ = new base::SimpleTestClock();
  GetHostContentSettingsMap()->SetPrefClockForTesting(
      std::unique_ptr<base::Clock>(clock_));
  clock_->SetNow(base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(10));

  // Setting the permission should trigger the last usage.
  GetHostContentSettingsMap()->SetContentSettingDefaultScope(
      current_url(), current_url(), CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(), CONTENT_SETTING_ALLOW);

  // Permission has been used at the starting time.
  EXPECT_EQ(GetHostContentSettingsMap()->GetLastUsage(
      current_url().GetOrigin(),
      current_url().GetOrigin(),
      CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(), 10);

  clock_->Advance(base::TimeDelta::FromSeconds(3));

  // Calling watchPosition should trigger the last usage update.
  WatchPositionAndObservePermissionBubble(false);
  ExpectPosition(fake_latitude(), fake_longitude());

  // Last usage has been updated.
  EXPECT_EQ(GetHostContentSettingsMap()->GetLastUsage(
      current_url().GetOrigin(),
      current_url().GetOrigin(),
      CONTENT_SETTINGS_TYPE_GEOLOCATION).ToDoubleT(), 13);
}
