// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/website_settings/permission_bubble_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_usages_state.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::DomOperationNotificationDetails;
using content::NavigationController;
using content::WebContents;

namespace {

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
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  NavigationController* controller = &web_contents->GetController();
  registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                 content::Source<NavigationController>(controller));
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
  std::string script(base::StringPrintf(
      "window.domAutomationController.setAutomationId(0);"
      "window.domAutomationController.send(addIFrame(%d, \"%s\"));",
      iframe_id, url.spec().c_str()));
  web_contents->GetMainFrame()->ExecuteJavaScript(base::UTF8ToUTF16(script));
  content::RunMessageLoop();

  EXPECT_EQ(base::StringPrintf("\"%d\"", iframe_id), javascript_response_);
  registrar_.RemoveAll();
  // Now that we loaded the iframe, let's fetch its src.
  script = base::StringPrintf(
      "window.domAutomationController.send(getIFrameSrc(%d))", iframe_id);
  std::string iframe_src;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(web_contents, script,
                                                     &iframe_src));
  iframe_url_ = GURL(iframe_src);
}

IFrameLoader::~IFrameLoader() {
}

void IFrameLoader::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_LOAD_STOP) {
    navigation_completed_ = true;
  } else if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
    content::Details<DomOperationNotificationDetails> dom_op_details(details);
    javascript_response_ = dom_op_details->json;
    javascript_completed_ = true;
  }
  if (javascript_completed_ && navigation_completed_)
    base::MessageLoopForUI::current()->Quit();
}

// PermissionRequestObserver ---------------------------------------------------

// Used to observe the creation of permission prompt without responding.
class PermissionRequestObserver : public PermissionBubbleManager::Observer {
 public:
  explicit PermissionRequestObserver(content::WebContents* web_contents)
      : bubble_manager_(PermissionBubbleManager::FromWebContents(web_contents)),
        request_shown_(false),
        message_loop_runner_(new content::MessageLoopRunner) {
    bubble_manager_->AddObserver(this);
  }
  ~PermissionRequestObserver() override {
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

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestObserver);
};

}  // namespace


// GeolocationBrowserTest -----------------------------------------------------

// This is a browser test for Geolocation.
// It exercises various integration points from javascript <-> browser:
// 1. Prompt is displayed when a geolocation is requested from an unauthorized
//    origin.
// 2. Denying the request triggers the correct error callback.
// 3. Allowing the request does not trigger an error, and allow a geoposition to
//    be passed to javascript.
// 4. Permissions persisted in disk are respected.
// 5. Incognito profiles don't use saved permissions.
class GeolocationBrowserTest : public InProcessBrowserTest {
 public:
  enum InitializationOptions {
    INITIALIZATION_NONE,
    INITIALIZATION_OFFTHERECORD,
    INITIALIZATION_NEWTAB,
    INITIALIZATION_IFRAMES,
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
  content::RenderFrameHost* frame_host() const { return render_frame_host_; }
  const GURL& current_url() const { return current_url_; }
  const GURL& iframe_url(size_t i) const { return iframe_urls_[i]; }
  double fake_latitude() const { return fake_latitude_; }
  double fake_longitude() const { return fake_longitude_; }

  // Initializes the test server and navigates to the initial url.
  bool Initialize(InitializationOptions options) WARN_UNUSED_RESULT;

  // Loads the specified number of iframes.
  void LoadIFrames(int number_iframes);

  // Specifies which frame is to be used for JavaScript calls.
  void SetFrameHost(const std::string& frame_name);

  // Check geolocation and accept or deny the resulting permission bubble.
  // Returns |true| if the expected behavior happened.
  bool RequestAndAcceptPermission() WARN_UNUSED_RESULT;
  bool RequestAndDenyPermission() WARN_UNUSED_RESULT;

  // Check geolocation and observe whether the permission bubble was shown.
  // Callers should set |bubble_should_display| to |true| if they expect a
  // bubble to display.
  void RequestPermissionAndObserve(bool bubble_should_display);

  // Checks that no errors have been received in javascript, and checks that the
  // position most recently received in javascript matches |latitude| and
  // |longitude|.
  void CheckGeoposition(double latitude, double longitude);

  // Checks whether a coordinate change has been registered yet (and waits if
  // it hasn't). After sending new geoposition coordinates, call this before
  // CheckGeoposition to avoid a race condition.
  bool CheckGeopositionUpdated() WARN_UNUSED_RESULT;

  // Executes |function| in |render_frame_host| and checks that the return value
  // matches |expected|.
  void CheckStringValueFromJavascriptForFrame(
      const std::string& expected,
      const std::string& function,
      content::RenderFrameHost* render_frame_host);

  // Executes |function| and checks that the return value matches |expected|.
  void CheckStringValueFromJavascript(const std::string& expected,
                                      const std::string& function);

  // Sets a new position and sends a notification with the new position. Call
  // CheckGeopositionReadyCallback after this to make sure that the value has
  // been received by the JavaScript.
  void NotifyGeoposition(double latitude, double longitude);

  // Convenience method to look up the number of queued permission bubbles.
  int GetBubblesQueueSize(PermissionBubbleManager* mgr);

 private:
  std::string RequestAndRespondToPermission(
      PermissionBubbleManager::AutoResponseType bubble_response);

  Browser* current_browser_;
  // path element of a URL referencing the html content for this test.
  std::string html_for_tests_;
  // This member defines the frame where the JavaScript calls will run.
  content::RenderFrameHost* render_frame_host_;
  // The current url for the top level page.
  GURL current_url_;
  // If not empty, the GURLs for the iframes loaded by LoadIFrames().
  std::vector<GURL> iframe_urls_;
  double fake_latitude_;
  double fake_longitude_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationBrowserTest);
};

GeolocationBrowserTest::GeolocationBrowserTest()
    : current_browser_(nullptr),
      html_for_tests_("/geolocation/simple.html"),
      render_frame_host_(nullptr),
      fake_latitude_(1.23),
      fake_longitude_(4.56) {
}

GeolocationBrowserTest::~GeolocationBrowserTest() {
}

void GeolocationBrowserTest::SetUpOnMainThread() {
  ui_test_utils::OverrideGeolocation(fake_latitude_, fake_longitude_);
}

void GeolocationBrowserTest::TearDownInProcessBrowserTestFixture() {
  LOG(WARNING) << "TearDownInProcessBrowserTestFixture. Test Finished.";
}

bool GeolocationBrowserTest::Initialize(InitializationOptions options) {
  if (!embedded_test_server()->Started() &&
      !embedded_test_server()->InitializeAndWaitUntilReady()) {
    ADD_FAILURE() << "Test server failed to start.";
    return false;
  }

  current_url_ = embedded_test_server()->GetURL(html_for_tests_);
  if (options == INITIALIZATION_OFFTHERECORD) {
    current_browser_ = OpenURLOffTheRecord(browser()->profile(), current_url_);
  } else {
    current_browser_ = browser();
    if (options == INITIALIZATION_NEWTAB)
      chrome::NewTab(current_browser_);
  }
  if (options != INITIALIZATION_OFFTHERECORD)
    ui_test_utils::NavigateToURL(current_browser_, current_url_);

  EXPECT_TRUE(current_browser_);
  return !!current_browser_;
}

void GeolocationBrowserTest::LoadIFrames(int number_iframes) {
  // Limit to 3 iframes.
  DCHECK_LT(0, number_iframes);
  DCHECK_LE(number_iframes, 3);
  iframe_urls_.resize(number_iframes);
  for (int i = 0; i < number_iframes; ++i) {
    IFrameLoader loader(current_browser_, i, GURL());
    iframe_urls_[i] = loader.iframe_url();
  }
}

void GeolocationBrowserTest::SetFrameHost(const std::string& frame_name) {
  WebContents* web_contents =
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

bool GeolocationBrowserTest::RequestAndAcceptPermission() {
  std::string result =
      RequestAndRespondToPermission(PermissionBubbleManager::ACCEPT_ALL);
  return "request-callback-success" == result;
}

bool GeolocationBrowserTest::RequestAndDenyPermission() {
  std::string result =
      RequestAndRespondToPermission(PermissionBubbleManager::DENY_ALL);
  return "request-callback-error" == result;
}

std::string GeolocationBrowserTest::RequestAndRespondToPermission(
    PermissionBubbleManager::AutoResponseType bubble_response) {
  std::string result;
  PermissionBubbleManager::FromWebContents(
      current_browser_->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(bubble_response);
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      render_frame_host_, "geoStartWithAsyncResponse();", &result));
  return result;
}

void GeolocationBrowserTest::RequestPermissionAndObserve(
    bool bubble_should_display) {
  std::string result;
  PermissionRequestObserver observer(
      current_browser_->tab_strip_model()->GetActiveWebContents());
  if (bubble_should_display) {
    // Control will return as soon as the API call is made, and then the
    // observer will wait for the bubble to display.
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        render_frame_host_, "geoStartWithSyncResponse()", &result));
    observer.Wait();
  } else {
    // Control will return once one of the callbacks fires.
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        render_frame_host_, "geoStartWithAsyncResponse()", &result));
  }
  EXPECT_EQ(bubble_should_display, observer.request_shown());
}

void GeolocationBrowserTest::CheckGeoposition(double latitude,
                                              double longitude) {
  // Checks we have no error.
  CheckStringValueFromJavascript("0", "geoGetLastError()");
  CheckStringValueFromJavascript(base::DoubleToString(latitude),
                                 "geoGetLastPositionLatitude()");
  CheckStringValueFromJavascript(base::DoubleToString(longitude),
                                 "geoGetLastPositionLongitude()");
}

bool GeolocationBrowserTest::CheckGeopositionUpdated() {
  // Control will return (a) if the update has already been received, or (b)
  // when the update is received. This will hang if the geolocation is never
  // updated. Currently this expects geoposition to be updated once; if your
  // test updates geoposition repeatedly, |position_updated| (JS) needs to
  // change to an int to count how often it's been updated.
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      render_frame_host_, "checkIfGeopositionUpdated();", &result));
  return result == "geoposition-updated";
}

void GeolocationBrowserTest::CheckStringValueFromJavascriptForFrame(
    const std::string& expected,
    const std::string& function,
    content::RenderFrameHost* render_frame_host) {
  std::string script(base::StringPrintf(
      "window.domAutomationController.send(%s)", function.c_str()));
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      render_frame_host, script, &result));
  EXPECT_EQ(expected, result);
}

void GeolocationBrowserTest::CheckStringValueFromJavascript(
    const std::string& expected,
    const std::string& function) {
  CheckStringValueFromJavascriptForFrame(
      expected, function, render_frame_host_);
}

void GeolocationBrowserTest::NotifyGeoposition(double latitude,
                                               double longitude) {
  fake_latitude_ = latitude;
  fake_longitude_ = longitude;
  ui_test_utils::OverrideGeolocation(latitude, longitude);
}

int GeolocationBrowserTest::GetBubblesQueueSize(PermissionBubbleManager* mgr) {
  return static_cast<int>(mgr->requests_.size());
}

// Tests ----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DisplaysPrompt) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  ASSERT_TRUE(RequestAndAcceptPermission());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, Geoposition) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, ErrorOnPermissionDenied) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  EXPECT_TRUE(RequestAndDenyPermission());
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForSecondTab) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  ASSERT_TRUE(RequestAndAcceptPermission());

  // Checks bubble is not needed in a second tab.
  ASSERT_TRUE(Initialize(INITIALIZATION_NEWTAB));
  SetFrameHost("");
  RequestPermissionAndObserve(false);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForDeniedOrigin) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  current_browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_BLOCK);

  // Check that the bubble wasn't shown but we get an error for this origin.
  SetFrameHost("");
  RequestPermissionAndObserve(false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");

  // Checks prompt will not be created a second tab.
  ASSERT_TRUE(Initialize(INITIALIZATION_NEWTAB));
  SetFrameHost("");
  RequestPermissionAndObserve(false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForAllowedOrigin) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  current_browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);
  // Checks no prompt will be created and there's no error callback.
  SetFrameHost("");
  RequestPermissionAndObserve(false);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptForOffTheRecord) {
  // Check prompt will be created and persisted for regular profile.
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Go incognito, and check no prompt will be created.
  ASSERT_TRUE(Initialize(INITIALIZATION_OFFTHERECORD));
  SetFrameHost("");
  RequestPermissionAndObserve(false);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoLeakFromOffTheRecord) {
  // Check prompt will be created for incognito profile.
  ASSERT_TRUE(Initialize(INITIALIZATION_OFFTHERECORD));
  SetFrameHost("");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Check prompt will be created for the regular profile.
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, IFramesWithFreshPosition) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  // Request permission from the first frame.
  SetFrameHost("iframe_0");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Test second iframe from a different origin with a cached geoposition will
  // create the prompt.
  SetFrameHost("iframe_1");
  RequestPermissionAndObserve(true);

  // Back to the first frame, enable navigation and refresh geoposition.
  SetFrameHost("iframe_0");
  double fresh_position_latitude = 3.17;
  double fresh_position_longitude = 4.23;
  NotifyGeoposition(fresh_position_latitude, fresh_position_longitude);
  ASSERT_TRUE(CheckGeopositionUpdated());
  CheckGeoposition(fresh_position_latitude, fresh_position_longitude);

  // Authorize the second frame and check it works.
  SetFrameHost("iframe_1");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, IFramesWithCachedPosition) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  // Request permission from the first frame.
  SetFrameHost("iframe_0");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Refresh geoposition, but let's not yet create the watch on the second frame
  // so that it'll fetch from cache.
  double cached_position_latitude = 5.67;
  double cached_position_lognitude = 8.09;
  NotifyGeoposition(cached_position_latitude, cached_position_lognitude);
  ASSERT_TRUE(CheckGeopositionUpdated());
  CheckGeoposition(cached_position_latitude, cached_position_lognitude);

  // Now check the second frame gets cached values as well.
  SetFrameHost("iframe_1");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(cached_position_latitude, cached_position_lognitude);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, CancelPermissionForFrame) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  SetFrameHost("iframe_0");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Test second iframe from a different origin with a cached geoposition will
  // create the prompt.
  SetFrameHost("iframe_1");
  RequestPermissionAndObserve(true);

  // Navigate the iframe, and ensure the prompt is gone.
  WebContents* web_contents =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  IFrameLoader change_iframe_1(current_browser(), 1, current_url());
  int num_bubbles_after_cancel = GetBubblesQueueSize(
      PermissionBubbleManager::FromWebContents(web_contents));
  EXPECT_EQ(0, num_bubbles_after_cancel);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, InvalidUrlRequest) {
  // Tests that an invalid URL (e.g. from a popup window) is rejected
  // correctly. Also acts as a regression test for http://crbug.com/40478
  set_html_for_tests("/geolocation/invalid_request_url.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));

  SetFrameHost("");
  WebContents* original_tab =
      current_browser()->tab_strip_model()->GetActiveWebContents();
  CheckStringValueFromJavascript("1", "requestGeolocationFromInvalidUrl()");
  CheckStringValueFromJavascriptForFrame("1", "isAlive()",
                                         original_tab->GetMainFrame());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoPromptBeforeStart) {
  // See http://crbug.com/42789
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  // Access navigator.geolocation, but ensure it won't request permission.
  SetFrameHost("iframe_1");
  CheckStringValueFromJavascript("object", "geoAccessNavigatorGeolocation()");

  SetFrameHost("iframe_0");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Permission should be requested after adding a watch.
  SetFrameHost("iframe_1");
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TwoWatchesInOneFrame) {
  set_html_for_tests("/geolocation/two_watches.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");

  // Tell the JS what to expect as the final coordinates.
  double final_position_latitude = 3.17;
  double final_position_longitude = 4.23;
  std::string script =
      base::StringPrintf("geoSetFinalPosition(%f, %f)", final_position_latitude,
                         final_position_longitude);
  CheckStringValueFromJavascript("ok", script);

  // Request permission and set two watches for the initial success callback.
  ASSERT_TRUE(RequestAndAcceptPermission());
  CheckGeoposition(fake_latitude(), fake_longitude());

  // The second watch will now have cancelled. Ensure an update still makes
  // its way through to the first watcher.
  NotifyGeoposition(final_position_latitude, final_position_longitude);
  ASSERT_TRUE(CheckGeopositionUpdated());
  CheckGeoposition(final_position_latitude, final_position_longitude);
}

// TODO(felt): Disabled because the second permission request hangs.
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DISABLED_PendingChildFrames) {
  set_html_for_tests("/geolocation/two_iframes.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  SetFrameHost("iframe_0");
  RequestPermissionAndObserve(true);

  SetFrameHost("iframe_1");
  RequestPermissionAndObserve(true);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TabDestroyed) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  RequestPermissionAndObserve(true);

  std::string script =
      "window.domAutomationController.send(window.close());";
  bool result = content::ExecuteScript(
      current_browser()->tab_strip_model()->GetActiveWebContents(), script);
  EXPECT_EQ(result, true);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, LastUsageUpdated) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  base::SimpleTestClock* clock_ = new base::SimpleTestClock();
  current_browser()
      ->profile()
      ->GetHostContentSettingsMap()
      ->SetPrefClockForTesting(scoped_ptr<base::Clock>(clock_));
  clock_->SetNow(base::Time::UnixEpoch() + base::TimeDelta::FromSeconds(10));

  // Setting the permission should trigger the last usage.
  current_browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      CONTENT_SETTING_ALLOW);

  // Permission has been used at the starting time.
  EXPECT_EQ(current_browser()
                ->profile()
                ->GetHostContentSettingsMap()
                ->GetLastUsage(current_url().GetOrigin(),
                               current_url().GetOrigin(),
                               CONTENT_SETTINGS_TYPE_GEOLOCATION)
                .ToDoubleT(),
            10);

  clock_->Advance(base::TimeDelta::FromSeconds(3));

  // Watching should trigger the last usage update.
  SetFrameHost("");
  RequestPermissionAndObserve(false);
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Last usage has been updated.
  EXPECT_EQ(current_browser()
                ->profile()
                ->GetHostContentSettingsMap()
                ->GetLastUsage(current_url().GetOrigin(),
                               current_url().GetOrigin(),
                               CONTENT_SETTINGS_TYPE_GEOLOCATION)
                .ToDoubleT(),
            13);
}
