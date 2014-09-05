// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/content_settings_usages_state.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
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
  virtual ~IFrameLoader();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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

// GeolocationNotificationObserver --------------------------------------------

class GeolocationNotificationObserver : public content::NotificationObserver {
 public:
  // If |wait_for_infobar| is true, AddWatchAndWaitForNotification will block
  // until the infobar has been displayed; otherwise it will block until the
  // navigation is completed.
  explicit GeolocationNotificationObserver(bool wait_for_infobar);
  virtual ~GeolocationNotificationObserver();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void AddWatchAndWaitForNotification(
      content::RenderFrameHost* render_frame_host);

  bool has_infobar() const { return !!infobar_; }
  infobars::InfoBar* infobar() { return infobar_; }

 private:
  content::NotificationRegistrar registrar_;
  bool wait_for_infobar_;
  infobars::InfoBar* infobar_;
  bool navigation_started_;
  bool navigation_completed_;
  std::string javascript_response_;

  DISALLOW_COPY_AND_ASSIGN(GeolocationNotificationObserver);
};

GeolocationNotificationObserver::GeolocationNotificationObserver(
    bool wait_for_infobar)
    : wait_for_infobar_(wait_for_infobar),
      infobar_(NULL),
      navigation_started_(false),
      navigation_completed_(false) {
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
  if (wait_for_infobar) {
    registrar_.Add(this, chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED,
                   content::NotificationService::AllSources());
  } else {
    registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::NotificationService::AllSources());
  }
}

GeolocationNotificationObserver::~GeolocationNotificationObserver() {
}

void GeolocationNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED) {
    infobar_ = content::Details<infobars::InfoBar::AddedDetails>(details).ptr();
    ASSERT_FALSE(infobar_->delegate()->GetIcon().IsEmpty());
    ASSERT_TRUE(infobar_->delegate()->AsConfirmInfoBarDelegate());
  } else if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
    content::Details<DomOperationNotificationDetails> dom_op_details(details);
    javascript_response_ = dom_op_details->json;
    LOG(WARNING) << "javascript_response " << javascript_response_;
  } else if ((type == content::NOTIFICATION_NAV_ENTRY_COMMITTED) ||
             (type == content::NOTIFICATION_LOAD_START)) {
    navigation_started_ = true;
  } else if ((type == content::NOTIFICATION_LOAD_STOP) && navigation_started_) {
    navigation_started_ = false;
    navigation_completed_ = true;
  }

  // We're either waiting for just the infobar, or for both a javascript
  // prompt and response.
  if ((wait_for_infobar_ && infobar_) ||
      (navigation_completed_ && !javascript_response_.empty()))
    base::MessageLoopForUI::current()->Quit();
}

void GeolocationNotificationObserver::AddWatchAndWaitForNotification(
    content::RenderFrameHost* render_frame_host) {
  LOG(WARNING) << "will add geolocation watch";
  std::string script(
      "window.domAutomationController.setAutomationId(0);"
      "window.domAutomationController.send(geoStart());");
  render_frame_host->ExecuteJavaScript(base::UTF8ToUTF16(script));
  content::RunMessageLoop();
  registrar_.RemoveAll();
  LOG(WARNING) << "got geolocation watch" << javascript_response_;
  EXPECT_NE("\"0\"", javascript_response_);
  EXPECT_TRUE(wait_for_infobar_ ? (infobar_ != NULL) : navigation_completed_);
}

}  // namespace


// GeolocationBrowserTest -----------------------------------------------------

// This is a browser test for Geolocation.
// It exercises various integration points from javascript <-> browser:
// 1. Infobar is displayed when a geolocation is requested from an unauthorized
// origin.
// 2. Denying the infobar triggers the correct error callback.
// 3. Allowing the infobar does not trigger an error, and allow a geoposition to
// be passed to javascript.
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
  virtual ~GeolocationBrowserTest();

  // InProcessBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE;
  virtual void TearDownInProcessBrowserTestFixture() OVERRIDE;

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

  // Start watching for geolocation notifications. If |wait_for_infobar| is
  // true, wait for the infobar to be displayed. Otherwise wait for a javascript
  // response.
  void AddGeolocationWatch(bool wait_for_infobar);

  // Checks that no errors have been received in javascript, and checks that the
  // position most recently received in javascript matches |latitude| and
  // |longitude|.
  void CheckGeoposition(double latitude, double longitude);

  // For |requesting_url| if |allowed| is true accept the infobar. Otherwise
  // cancel it.
  void SetInfoBarResponse(const GURL& requesting_url, bool allowed);

  // Executes |function| in |render_frame_host| and checks that the return value
  // matches |expected|.
  void CheckStringValueFromJavascriptForFrame(
      const std::string& expected,
      const std::string& function,
      content::RenderFrameHost* render_frame_host);

  // Executes |function| and checks that the return value matches |expected|.
  void CheckStringValueFromJavascript(const std::string& expected,
                                      const std::string& function);

  // Sets a new position and sends a notification with the new position.
  void NotifyGeoposition(double latitude, double longitude);

 private:
  infobars::InfoBar* infobar_;
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
  : infobar_(NULL),
    current_browser_(NULL),
    html_for_tests_("/geolocation/simple.html"),
    render_frame_host_(NULL),
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
  LOG(WARNING) << "before navigate";
  if (options == INITIALIZATION_OFFTHERECORD) {
    current_browser_ = ui_test_utils::OpenURLOffTheRecord(
        browser()->profile(), current_url_);
  } else {
    current_browser_ = browser();
    if (options == INITIALIZATION_NEWTAB)
      chrome::NewTab(current_browser_);
    ui_test_utils::NavigateToURL(current_browser_, current_url_);
  }
  LOG(WARNING) << "after navigate";

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
  render_frame_host_ = NULL;

  if (frame_name.empty()) {
    render_frame_host_ = web_contents->GetMainFrame();
  } else {
    render_frame_host_ = content::FrameMatchingPredicate(
        web_contents, base::Bind(&content::FrameMatchesName, frame_name));
  }
  DCHECK(render_frame_host_);
}

void GeolocationBrowserTest::AddGeolocationWatch(bool wait_for_infobar) {
  GeolocationNotificationObserver notification_observer(wait_for_infobar);
  notification_observer.AddWatchAndWaitForNotification(render_frame_host_);
  if (wait_for_infobar) {
    EXPECT_TRUE(notification_observer.has_infobar());
    infobar_ = notification_observer.infobar();
  }
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

void GeolocationBrowserTest::SetInfoBarResponse(const GURL& requesting_url,
                                                bool allowed) {
  WebContents* web_contents =
      current_browser_->tab_strip_model()->GetActiveWebContents();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  const ContentSettingsUsagesState& usages_state =
      content_settings->geolocation_usages_state();
  size_t state_map_size = usages_state.state_map().size();
  ASSERT_TRUE(infobar_);
  LOG(WARNING) << "will set infobar response";
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::Source<NavigationController>(&web_contents->GetController()));
    if (allowed)
      infobar_->delegate()->AsConfirmInfoBarDelegate()->Accept();
    else
      infobar_->delegate()->AsConfirmInfoBarDelegate()->Cancel();
    observer.Wait();
  }

  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  infobar_service->RemoveInfoBar(infobar_);
  LOG(WARNING) << "infobar response set";
  infobar_ = NULL;
  EXPECT_GT(usages_state.state_map().size(), state_map_size);
  GURL requesting_origin(requesting_url.GetOrigin());
  EXPECT_EQ(1U, usages_state.state_map().count(requesting_origin));
  ContentSetting expected_setting =
        allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
  EXPECT_EQ(expected_setting,
            usages_state.state_map().find(requesting_origin)->second);
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
  LOG(WARNING) << "MockLocationProvider listeners updated";
}


// Tests ----------------------------------------------------------------------

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DisplaysPermissionBar) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  AddGeolocationWatch(true);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, Geoposition) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  AddGeolocationWatch(true);
  SetInfoBarResponse(current_url(), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       ErrorOnPermissionDenied) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  AddGeolocationWatch(true);
  // Infobar was displayed, deny access and check for error code.
  SetInfoBarResponse(current_url(), false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

// See http://crbug.com/308358
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DISABLED_NoInfobarForSecondTab) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  AddGeolocationWatch(true);
  SetInfoBarResponse(current_url(), true);
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Checks infobar will not be created in a second tab.
  ASSERT_TRUE(Initialize(INITIALIZATION_NEWTAB));
  SetFrameHost("");
  AddGeolocationWatch(false);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForDeniedOrigin) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  current_browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_BLOCK);
  SetFrameHost("");
  AddGeolocationWatch(false);
  // Checks we have an error for this denied origin.
  CheckStringValueFromJavascript("1", "geoGetLastError()");
  // Checks infobar will not be created a second tab.
  ASSERT_TRUE(Initialize(INITIALIZATION_NEWTAB));
  SetFrameHost("");
  AddGeolocationWatch(false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForAllowedOrigin) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  current_browser()->profile()->GetHostContentSettingsMap()->SetContentSetting(
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      ContentSettingsPattern::FromURLNoWildcard(current_url()),
      CONTENT_SETTINGS_TYPE_GEOLOCATION, std::string(), CONTENT_SETTING_ALLOW);
  // Checks no infobar will be created and there's no error callback.
  SetFrameHost("");
  AddGeolocationWatch(false);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForOffTheRecord) {
  // First, check infobar will be created for regular profile
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  AddGeolocationWatch(true);
  // Response will be persisted.
  SetInfoBarResponse(current_url(), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");
  // Go incognito, and checks no infobar will be created.
  ASSERT_TRUE(Initialize(INITIALIZATION_OFFTHERECORD));
  SetFrameHost("");
  AddGeolocationWatch(false);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoLeakFromOffTheRecord) {
  // First, check infobar will be created for incognito profile.
  ASSERT_TRUE(Initialize(INITIALIZATION_OFFTHERECORD));
  SetFrameHost("");
  AddGeolocationWatch(true);
  // Response won't be persisted.
  SetInfoBarResponse(current_url(), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");
  // Go to the regular profile, infobar will be created.
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  SetFrameHost("");
  AddGeolocationWatch(true);
  SetInfoBarResponse(current_url(), false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, IFramesWithFreshPosition) {
  set_html_for_tests("/geolocation/iframes_different_origin.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);
  LOG(WARNING) << "frames loaded";

  SetFrameHost("iframe_0");
  AddGeolocationWatch(true);
  SetInfoBarResponse(iframe_url(0), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
  // Disables further prompts from this iframe.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Test second iframe from a different origin with a cached geoposition will
  // create the infobar.
  SetFrameHost("iframe_1");
  AddGeolocationWatch(true);

  // Back to the first frame, enable navigation and refresh geoposition.
  SetFrameHost("iframe_0");
  CheckStringValueFromJavascript("1", "geoSetMaxNavigateCount(1)");
  double fresh_position_latitude = 3.17;
  double fresh_position_longitude = 4.23;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &current_browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  NotifyGeoposition(fresh_position_latitude, fresh_position_longitude);
  observer.Wait();
  CheckGeoposition(fresh_position_latitude, fresh_position_longitude);

  // Disable navigation for this frame.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Now go ahead an authorize the second frame.
  SetFrameHost("iframe_1");
  // Infobar was displayed, allow access and check there's no error code.
  SetInfoBarResponse(iframe_url(1), true);
  LOG(WARNING) << "Checking position...";
  CheckGeoposition(fresh_position_latitude, fresh_position_longitude);
  LOG(WARNING) << "...done.";
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       IFramesWithCachedPosition) {
  set_html_for_tests("/geolocation/iframes_different_origin.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  SetFrameHost("iframe_0");
  AddGeolocationWatch(true);
  SetInfoBarResponse(iframe_url(0), true);
  CheckGeoposition(fake_latitude(), fake_longitude());

  // Refresh geoposition, but let's not yet create the watch on the second frame
  // so that it'll fetch from cache.
  double cached_position_latitude = 5.67;
  double cached_position_lognitude = 8.09;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &current_browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  NotifyGeoposition(cached_position_latitude, cached_position_lognitude);
  observer.Wait();
  CheckGeoposition(cached_position_latitude, cached_position_lognitude);

  // Disable navigation for this frame.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Now go ahead and authorize the second frame.
  SetFrameHost("iframe_1");
  AddGeolocationWatch(true);
  // WebKit will use its cache, but we also broadcast a position shortly
  // afterwards. We're only interested in the first navigation for the success
  // callback from the cached position.
  CheckStringValueFromJavascript("1", "geoSetMaxNavigateCount(1)");
  SetInfoBarResponse(iframe_url(1), true);
  CheckGeoposition(cached_position_latitude, cached_position_lognitude);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, CancelPermissionForFrame) {
  set_html_for_tests("/geolocation/iframes_different_origin.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);
  LOG(WARNING) << "frames loaded";

  SetFrameHost("iframe_0");
  AddGeolocationWatch(true);
  SetInfoBarResponse(iframe_url(0), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
  // Disables further prompts from this iframe.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Test second iframe from a different origin with a cached geoposition will
  // create the infobar.
  SetFrameHost("iframe_1");
  AddGeolocationWatch(true);

  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      current_browser()->tab_strip_model()->GetActiveWebContents());
  size_t num_infobars_before_cancel = infobar_service->infobar_count();
  // Change the iframe, and ensure the infobar is gone.
  IFrameLoader change_iframe_1(current_browser(), 1, current_url());
  size_t num_infobars_after_cancel = infobar_service->infobar_count();
  EXPECT_EQ(num_infobars_before_cancel, num_infobars_after_cancel + 1);
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

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfoBarBeforeStart) {
  // See http://crbug.com/42789
  set_html_for_tests("/geolocation/iframes_different_origin.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);
  LOG(WARNING) << "frames loaded";

  // Access navigator.geolocation, but ensure it won't request permission.
  SetFrameHost("iframe_1");
  CheckStringValueFromJavascript("object", "geoAccessNavigatorGeolocation()");

  SetFrameHost("iframe_0");
  AddGeolocationWatch(true);
  SetInfoBarResponse(iframe_url(0), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Permission should be requested after adding a watch.
  SetFrameHost("iframe_1");
  AddGeolocationWatch(true);
  SetInfoBarResponse(iframe_url(1), true);
  CheckGeoposition(fake_latitude(), fake_longitude());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TwoWatchesInOneFrame) {
  set_html_for_tests("/geolocation/two_watches.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));

  // First, set the JavaScript to navigate when it receives |final_position|.
  double final_position_latitude = 3.17;
  double final_position_longitude = 4.23;
  std::string script = base::StringPrintf(
      "window.domAutomationController.send(geoSetFinalPosition(%f, %f))",
      final_position_latitude, final_position_longitude);
  std::string js_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      current_browser()->tab_strip_model()->GetActiveWebContents(), script,
      &js_result));
  EXPECT_EQ(js_result, "ok");

  // Send a position which both geolocation watches will receive.
  SetFrameHost("");
  AddGeolocationWatch(true);
  SetInfoBarResponse(current_url(), true);
  CheckGeoposition(fake_latitude(), fake_longitude());

  // The second watch will now have cancelled. Ensure an update still makes
  // its way through to the first watcher.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &current_browser()->tab_strip_model()->GetActiveWebContents()->
              GetController()));
  NotifyGeoposition(final_position_latitude, final_position_longitude);
  observer.Wait();
  CheckGeoposition(final_position_latitude, final_position_longitude);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TabDestroyed) {
  set_html_for_tests("/geolocation/tab_destroyed.html");
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(3);

  SetFrameHost("iframe_0");
  AddGeolocationWatch(true);

  SetFrameHost("iframe_1");
  AddGeolocationWatch(false);

  SetFrameHost("iframe_2");
  AddGeolocationWatch(false);

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
  AddGeolocationWatch(false);
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
