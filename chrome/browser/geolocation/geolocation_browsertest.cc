// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/compiler_specific.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/geolocation/geolocation_settings_state.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/content_settings_pattern.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"

using content::DomOperationNotificationDetails;
using content::NavigationController;
using content::WebContents;

namespace {

// Used to block until an iframe is loaded via a javascript call.
// Note: NavigateToURLBlockUntilNavigationsComplete doesn't seem to work for
// multiple embedded iframes, as notifications seem to be 'batched'. Instead, we
// load and wait one single frame here by calling a javascript function.
class IFrameLoader : public content::NotificationObserver {
 public:
  IFrameLoader(Browser* browser, int iframe_id, const GURL& url)
      : navigation_completed_(false),
        javascript_completed_(false) {
    NavigationController* controller =
        &chrome::GetActiveWebContents(browser)->GetController();
    registrar_.Add(this, content::NOTIFICATION_LOAD_STOP,
                   content::Source<NavigationController>(controller));
    registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::NotificationService::AllSources());
    std::string script = base::StringPrintf(
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send(addIFrame(%d, \"%s\"));",
        iframe_id,
        url.spec().c_str());
    chrome::GetActiveWebContents(browser)->GetRenderViewHost()->
        ExecuteJavascriptInWebFrame(string16(), UTF8ToUTF16(script));
    content::RunMessageLoop();

    EXPECT_EQ(base::StringPrintf("\"%d\"", iframe_id), javascript_response_);
    registrar_.RemoveAll();
    // Now that we loaded the iframe, let's fetch its src.
    script = base::StringPrintf(
        "window.domAutomationController.send(getIFrameSrc(%d))", iframe_id);
    std::string iframe_src;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        chrome::GetActiveWebContents(browser),
        script,
        &iframe_src));
    iframe_url_ = GURL(iframe_src);
  }

  GURL iframe_url() const { return iframe_url_; }

  virtual void Observe(int type,
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
      MessageLoopForUI::current()->Quit();
  }

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

class GeolocationNotificationObserver : public content::NotificationObserver {
 public:
  // If |wait_for_infobar| is true, AddWatchAndWaitForNotification will block
  // until the infobar has been displayed; otherwise it will block until the
  // navigation is completed.
  explicit GeolocationNotificationObserver(bool wait_for_infobar)
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

  void AddWatchAndWaitForNotification(content::RenderViewHost* render_view_host,
                                      const std::string& iframe_xpath) {
    LOG(WARNING) << "will add geolocation watch";
    std::string script =
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send(geoStart());";
    render_view_host->ExecuteJavascriptInWebFrame(UTF8ToUTF16(iframe_xpath),
                                                  UTF8ToUTF16(script));
    content::RunMessageLoop();
    registrar_.RemoveAll();
    LOG(WARNING) << "got geolocation watch" << javascript_response_;
    EXPECT_NE("\"0\"", javascript_response_);
    if (wait_for_infobar_) {
      EXPECT_TRUE(infobar_);
    } else {
      EXPECT_TRUE(navigation_completed_);
    }
  }

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_TAB_CONTENTS_INFOBAR_ADDED) {
      infobar_ = content::Details<InfoBarAddedDetails>(details).ptr();
      ASSERT_TRUE(infobar_->GetIcon());
      ASSERT_TRUE(infobar_->AsConfirmInfoBarDelegate());
    } else if (type == content::NOTIFICATION_DOM_OPERATION_RESPONSE) {
      content::Details<DomOperationNotificationDetails> dom_op_details(details);
      javascript_response_ = dom_op_details->json;
      LOG(WARNING) << "javascript_response " << javascript_response_;
    } else if (type == content::NOTIFICATION_NAV_ENTRY_COMMITTED ||
               type == content::NOTIFICATION_LOAD_START) {
      navigation_started_ = true;
    } else if (type == content::NOTIFICATION_LOAD_STOP) {
      if (navigation_started_) {
        navigation_started_ = false;
        navigation_completed_ = true;
      }
    }

    // We're either waiting for just the inforbar, or for both a javascript
    // prompt and response.
    if (wait_for_infobar_ && infobar_)
      MessageLoopForUI::current()->Quit();
    else if (navigation_completed_ && !javascript_response_.empty())
      MessageLoopForUI::current()->Quit();
  }

  content::NotificationRegistrar registrar_;
  bool wait_for_infobar_;
  InfoBarDelegate* infobar_;
  bool navigation_started_;
  bool navigation_completed_;
  std::string javascript_response_;
};

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
  GeolocationBrowserTest()
    : infobar_(NULL),
      current_browser_(NULL),
      html_for_tests_("files/geolocation/simple.html"),
      fake_latitude_(1.23),
      fake_longitude_(4.56),
      started_test_server_(false) {}

  // InProcessBrowserTest
  virtual void SetUpOnMainThread() {
    ui_test_utils::OverrideGeolocation(fake_latitude_, fake_longitude_);
  }

  // InProcessBrowserTest
  virtual void TearDownInProcessBrowserTestFixture() {
    LOG(WARNING) << "TearDownInProcessBrowserTestFixture. Test Finished.";
  }

  enum InitializationOptions {
    INITIALIZATION_NONE,
    INITIALIZATION_OFFTHERECORD,
    INITIALIZATION_NEWTAB,
    INITIALIZATION_IFRAMES,
  };

  bool Initialize(InitializationOptions options) WARN_UNUSED_RESULT {
    if (!started_test_server_)
      started_test_server_ = test_server()->Start();
    EXPECT_TRUE(started_test_server_);
    if (!started_test_server_)
      return false;

    current_url_ = test_server()->GetURL(html_for_tests_);
    LOG(WARNING) << "before navigate";
    if (options == INITIALIZATION_OFFTHERECORD) {
      current_browser_ = ui_test_utils::OpenURLOffTheRecord(
          browser()->profile(), current_url_);
    } else if (options == INITIALIZATION_NEWTAB) {
      current_browser_ = browser();
      chrome::NewTab(current_browser_);
      ui_test_utils::NavigateToURL(current_browser_, current_url_);
    } else if (options == INITIALIZATION_IFRAMES) {
      current_browser_ = browser();
      ui_test_utils::NavigateToURL(current_browser_, current_url_);
    } else {
      current_browser_ = browser();
      ui_test_utils::NavigateToURL(current_browser_, current_url_);
    }
    LOG(WARNING) << "after navigate";

    EXPECT_TRUE(current_browser_);
    if (!current_browser_)
      return false;

    return true;
  }

  void LoadIFrames(int number_iframes) {
      // Limit to 3 iframes.
      DCHECK(0 < number_iframes && number_iframes <= 3);
      iframe_urls_.resize(number_iframes);
      for (int i = 0; i < number_iframes; ++i) {
        IFrameLoader loader(current_browser_, i, GURL());
        iframe_urls_[i] = loader.iframe_url();
      }
  }

  void AddGeolocationWatch(bool wait_for_infobar) {
    GeolocationNotificationObserver notification_observer(wait_for_infobar);
    notification_observer.AddWatchAndWaitForNotification(
        chrome::GetActiveWebContents(current_browser_)->GetRenderViewHost(),
        iframe_xpath_);
    if (wait_for_infobar) {
      EXPECT_TRUE(notification_observer.infobar_);
      infobar_ = notification_observer.infobar_;
    }
  }

  void CheckGeoposition(double latitude, double longitude) {
    // Checks we have no error.
    CheckStringValueFromJavascript("0", "geoGetLastError()");
    CheckStringValueFromJavascript(base::DoubleToString(latitude),
                                   "geoGetLastPositionLatitude()");
    CheckStringValueFromJavascript(base::DoubleToString(longitude),
                                   "geoGetLastPositionLongitude()");
  }

  void SetInfobarResponse(const GURL& requesting_url, bool allowed) {
    WebContents* web_contents = chrome::GetActiveWebContents(current_browser_);
    TabSpecificContentSettings* content_settings =
        TabSpecificContentSettings::FromWebContents(web_contents);
    const GeolocationSettingsState& settings_state =
        content_settings->geolocation_settings_state();
    size_t state_map_size = settings_state.state_map().size();
    ASSERT_TRUE(infobar_);
    LOG(WARNING) << "will set infobar response";
    {
      content::WindowedNotificationObserver observer(
          content::NOTIFICATION_LOAD_STOP,
          content::Source<NavigationController>(
              &web_contents->GetController()));
      if (allowed)
        infobar_->AsConfirmInfoBarDelegate()->Accept();
      else
        infobar_->AsConfirmInfoBarDelegate()->Cancel();
      observer.Wait();
    }

    InfoBarService::FromWebContents(web_contents)->RemoveInfoBar(infobar_);
    LOG(WARNING) << "infobar response set";
    infobar_ = NULL;
    EXPECT_GT(settings_state.state_map().size(), state_map_size);
    GURL requesting_origin = requesting_url.GetOrigin();
    EXPECT_EQ(1U, settings_state.state_map().count(requesting_origin));
    ContentSetting expected_setting =
          allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    EXPECT_EQ(expected_setting,
              settings_state.state_map().find(requesting_origin)->second);
  }

  void CheckStringValueFromJavascriptForTab(
      const std::string& expected, const std::string& function,
      WebContents* web_contents) {
    std::string script = base::StringPrintf(
        "window.domAutomationController.send(%s)", function.c_str());
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptInFrameAndExtractString(
        web_contents,
        iframe_xpath_,
        script,
        &result));
    EXPECT_EQ(expected, result);
  }

  void CheckStringValueFromJavascript(
      const std::string& expected, const std::string& function) {
    CheckStringValueFromJavascriptForTab(
        expected, function, chrome::GetActiveWebContents(current_browser_));
  }

  void NotifyGeoposition(double latitude, double longitude) {
    fake_latitude_ = latitude;
    fake_longitude_ = longitude;
    ui_test_utils::OverrideGeolocation(latitude, longitude);
    LOG(WARNING) << "MockLocationProvider listeners updated";
  }

  InfoBarDelegate* infobar_;
  Browser* current_browser_;
  // path element of a URL referencing the html content for this test.
  std::string html_for_tests_;
  // This member defines the iframe (or top-level page, if empty) where the
  // javascript calls will run.
  std::string iframe_xpath_;
  // The current url for the top level page.
  GURL current_url_;
  // If not empty, the GURLs for the iframes loaded by LoadIFrames().
  std::vector<GURL> iframe_urls_;
  double fake_latitude_;
  double fake_longitude_;

  // TODO(phajdan.jr): Remove after we can ask TestServer whether it is started.
  bool started_test_server_;
};

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DisplaysPermissionBar) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  AddGeolocationWatch(true);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, Geoposition) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  AddGeolocationWatch(true);
  SetInfobarResponse(current_url_, true);
  CheckGeoposition(fake_latitude_, fake_longitude_);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       ErrorOnPermissionDenied) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  AddGeolocationWatch(true);
  // Infobar was displayed, deny access and check for error code.
  SetInfobarResponse(current_url_, false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

// http://crbug.com/44589. Hangs on Mac, crashes on Windows
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_NoInfobarForSecondTab DISABLED_NoInfobarForSecondTab
#else
#define MAYBE_NoInfobarForSecondTab NoInfobarForSecondTab
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoInfobarForSecondTab) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  AddGeolocationWatch(true);
  SetInfobarResponse(current_url_, true);
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Checks infobar will not be created a second tab.
  ASSERT_TRUE(Initialize(INITIALIZATION_NEWTAB));
  AddGeolocationWatch(false);
  CheckGeoposition(fake_latitude_, fake_longitude_);
}

// http://crbug.com/44589. Hangs on Mac, crashes on Windows
#if defined(OS_MACOSX) || defined(OS_WIN)
#define MAYBE_NoInfobarForDeniedOrigin DISABLED_NoInfobarForDeniedOrigin
#else
#define MAYBE_NoInfobarForDeniedOrigin NoInfobarForDeniedOrigin
#endif
IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoInfobarForDeniedOrigin) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  current_browser_->profile()->GetHostContentSettingsMap()->
      SetContentSetting(ContentSettingsPattern::FromURLNoWildcard(current_url_),
                        ContentSettingsPattern::FromURLNoWildcard(current_url_),
                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                        std::string(),
                        CONTENT_SETTING_BLOCK);
  AddGeolocationWatch(false);
  // Checks we have an error for this denied origin.
  CheckStringValueFromJavascript("1", "geoGetLastError()");
  // Checks infobar will not be created a second tab.
  ASSERT_TRUE(Initialize(INITIALIZATION_NEWTAB));
  AddGeolocationWatch(false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForAllowedOrigin) {
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  current_browser_->profile()->GetHostContentSettingsMap()->
      SetContentSetting(ContentSettingsPattern::FromURLNoWildcard(current_url_),
                        ContentSettingsPattern::FromURLNoWildcard(current_url_),
                        CONTENT_SETTINGS_TYPE_GEOLOCATION,
                        std::string(),
                        CONTENT_SETTING_ALLOW);
  // Checks no infobar will be created and there's no error callback.
  AddGeolocationWatch(false);
  CheckGeoposition(fake_latitude_, fake_longitude_);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForOffTheRecord) {
  // First, check infobar will be created for regular profile
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  AddGeolocationWatch(true);
  // Response will be persisted
  SetInfobarResponse(current_url_, true);
  CheckGeoposition(fake_latitude_, fake_longitude_);
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");
  // Go incognito, and checks no infobar will be created.
  ASSERT_TRUE(Initialize(INITIALIZATION_OFFTHERECORD));
  AddGeolocationWatch(false);
  CheckGeoposition(fake_latitude_, fake_longitude_);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       IFramesWithFreshPosition) {
  html_for_tests_ = "files/geolocation/iframes_different_origin.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);
  LOG(WARNING) << "frames loaded";

  iframe_xpath_ = "//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe_urls_[0], true);
  CheckGeoposition(fake_latitude_, fake_longitude_);
  // Disables further prompts from this iframe.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Test second iframe from a different origin with a cached geoposition will
  // create the infobar.
  iframe_xpath_ = "//iframe[@id='iframe_1']";
  AddGeolocationWatch(true);

  // Back to the first frame, enable navigation and refresh geoposition.
  iframe_xpath_ = "//iframe[@id='iframe_0']";
  CheckStringValueFromJavascript("1", "geoSetMaxNavigateCount(1)");
  double fresh_position_latitude = 3.17;
  double fresh_position_longitude = 4.23;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &chrome::GetActiveWebContents(current_browser_)->GetController()));
  NotifyGeoposition(fresh_position_latitude, fresh_position_longitude);
  observer.Wait();
  CheckGeoposition(fresh_position_latitude, fresh_position_longitude);

  // Disable navigation for this frame.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Now go ahead an authorize the second frame.
  iframe_xpath_ = "//iframe[@id='iframe_1']";
  // Infobar was displayed, allow access and check there's no error code.
  SetInfobarResponse(iframe_urls_[1], true);
  LOG(WARNING) << "Checking position...";
  CheckGeoposition(fresh_position_latitude, fresh_position_longitude);
  LOG(WARNING) << "...done.";
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       IFramesWithCachedPosition) {
  html_for_tests_ = "files/geolocation/iframes_different_origin.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);

  iframe_xpath_ = "//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe_urls_[0], true);
  CheckGeoposition(fake_latitude_, fake_longitude_);

  // Refresh geoposition, but let's not yet create the watch on the second frame
  // so that it'll fetch from cache.
  double cached_position_latitude = 5.67;
  double cached_position_lognitude = 8.09;
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &chrome::GetActiveWebContents(current_browser_)->GetController()));
  NotifyGeoposition(cached_position_latitude, cached_position_lognitude);
  observer.Wait();
  CheckGeoposition(cached_position_latitude, cached_position_lognitude);

  // Disable navigation for this frame.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Now go ahead an authorize the second frame.
  iframe_xpath_ = "//iframe[@id='iframe_1']";
  AddGeolocationWatch(true);
  // WebKit will use its cache, but we also broadcast a position shortly
  // afterwards. We're only interested in the first navigation for the success
  // callback from the cached position.
  CheckStringValueFromJavascript("1", "geoSetMaxNavigateCount(1)");
  SetInfobarResponse(iframe_urls_[1], true);
  CheckGeoposition(cached_position_latitude, cached_position_lognitude);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, CancelPermissionForFrame) {
  html_for_tests_ = "files/geolocation/iframes_different_origin.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);
  LOG(WARNING) << "frames loaded";

  iframe_xpath_ = "//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe_urls_[0], true);
  CheckGeoposition(fake_latitude_, fake_longitude_);
  // Disables further prompts from this iframe.
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Test second iframe from a different origin with a cached geoposition will
  // create the infobar.
  iframe_xpath_ = "//iframe[@id='iframe_1']";
  AddGeolocationWatch(true);

  InfoBarService* infobar_service = InfoBarService::FromWebContents(
      chrome::GetActiveWebContents(current_browser_));
  size_t num_infobars_before_cancel = infobar_service->GetInfoBarCount();
  // Change the iframe, and ensure the infobar is gone.
  IFrameLoader change_iframe_1(current_browser_, 1, current_url_);
  size_t num_infobars_after_cancel = infobar_service->GetInfoBarCount();
  EXPECT_EQ(num_infobars_before_cancel, num_infobars_after_cancel + 1);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, InvalidUrlRequest) {
  // Tests that an invalid URL (e.g. from a popup window) is rejected
  // correctly. Also acts as a regression test for http://crbug.com/40478
  html_for_tests_ = "files/geolocation/invalid_request_url.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  WebContents* original_tab = chrome::GetActiveWebContents(current_browser_);
  CheckStringValueFromJavascript("1", "requestGeolocationFromInvalidUrl()");
  CheckStringValueFromJavascriptForTab("1", "isAlive()", original_tab);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfoBarBeforeStart) {
  // See http://crbug.com/42789
  html_for_tests_ = "files/geolocation/iframes_different_origin.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(2);
  LOG(WARNING) << "frames loaded";

  // Access navigator.geolocation, but ensure it won't request permission.
  iframe_xpath_ = "//iframe[@id='iframe_1']";
  CheckStringValueFromJavascript("object", "geoAccessNavigatorGeolocation()");

  iframe_xpath_ = "//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe_urls_[0], true);
  CheckGeoposition(fake_latitude_, fake_longitude_);
  CheckStringValueFromJavascript("0", "geoSetMaxNavigateCount(0)");

  // Permission should be requested after adding a watch.
  iframe_xpath_ = "//iframe[@id='iframe_1']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe_urls_[1], true);
  CheckGeoposition(fake_latitude_, fake_longitude_);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TwoWatchesInOneFrame) {
  html_for_tests_ = "files/geolocation/two_watches.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_NONE));
  // First, set the JavaScript to navigate when it receives |final_position|.
  double final_position_latitude = 3.17;
  double final_position_longitude = 4.23;
  std::string script = base::StringPrintf(
      "window.domAutomationController.send(geoSetFinalPosition(%f, %f))",
      final_position_latitude, final_position_longitude);
  std::string js_result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      chrome::GetActiveWebContents(current_browser_),
      script,
      &js_result));
  EXPECT_EQ(js_result, "ok");

  // Send a position which both geolocation watches will receive.
  AddGeolocationWatch(true);
  SetInfobarResponse(current_url_, true);
  CheckGeoposition(fake_latitude_, fake_longitude_);

  // The second watch will now have cancelled. Ensure an update still makes
  // its way through to the first watcher.
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(
          &chrome::GetActiveWebContents(current_browser_)->GetController()));
  NotifyGeoposition(final_position_latitude, final_position_longitude);
  observer.Wait();
  CheckGeoposition(final_position_latitude, final_position_longitude);
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, TabDestroyed) {
  html_for_tests_ = "files/geolocation/tab_destroyed.html";
  ASSERT_TRUE(Initialize(INITIALIZATION_IFRAMES));
  LoadIFrames(3);

  iframe_xpath_ = "//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);

  iframe_xpath_ = "//iframe[@id='iframe_1']";
  AddGeolocationWatch(false);

  iframe_xpath_ = "//iframe[@id='iframe_2']";
  AddGeolocationWatch(false);

  std::string script =
      "window.domAutomationController.send(window.close());";
  bool result =
      content::ExecuteScript(
          chrome::GetActiveWebContents(current_browser_),
          script);
  EXPECT_EQ(result, true);
}

}  // namespace
