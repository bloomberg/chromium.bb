// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/waitable_event.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
#include "chrome/browser/geolocation/location_provider.h"
#include "chrome/browser/geolocation/mock_location_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/geoposition.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/in_process_browser_test.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/net_util.h"

// Used to block until an iframe is loaded via a javascript call.
// Note: NavigateToURLBlockUntilNavigationsComplete doesn't seem to work for
// multiple embedded iframes, as notifications seem to be 'batched'. Instead, we
// load and wait one single frame here by calling a javascript function.
class IFrameLoader : public NotificationObserver {
 public:
   IFrameLoader(Browser* browser, int iframe_id)
      : navigation_completed_(false),
        javascript_completed_(false) {
    NavigationController* controller =
        &browser->GetSelectedTabContents()->controller();
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                   NotificationService::AllSources());
    std::string script = StringPrintf(
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send(addIFrame(%d));",
        iframe_id);
    browser->GetSelectedTabContents()->render_view_host()->
        ExecuteJavascriptInWebFrame(L"", UTF8ToWide(script));
    ui_test_utils::RunMessageLoop();

    EXPECT_EQ(StringPrintf("\"%d\"", iframe_id), javascript_response_);
    registrar_.RemoveAll();
    // Now that we loaded the iframe, let's fetch its src.
    script = StringPrintf(
        "window.domAutomationController.send(getIFrameSrc(%d))", iframe_id);
    std::string iframe_src;
    ui_test_utils::ExecuteJavaScriptAndExtractString(
        browser->GetSelectedTabContents()->render_view_host(),
        L"", UTF8ToWide(script), &iframe_src);
    iframe_url_ = GURL(iframe_src);
  }

  GURL iframe_url() const { return iframe_url_; }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::LOAD_STOP) {
      navigation_completed_ = true;
    } else if (type == NotificationType::DOM_OPERATION_RESPONSE) {
      Details<DomOperationNotificationDetails> dom_op_details(details);
      javascript_response_ = dom_op_details->json();
      javascript_completed_ = true;
    }
    if (javascript_completed_ && navigation_completed_)
      MessageLoopForUI::current()->Quit();
  }

 private:
  NotificationRegistrar registrar_;

  // If true the navigation has completed.
  bool navigation_completed_;

  // If true the javascript call has completed.
  bool javascript_completed_;

  std::string javascript_response_;

  // The URL for the iframe we just loaded.
  GURL iframe_url_;

  DISALLOW_COPY_AND_ASSIGN(IFrameLoader);
};

class GeolocationNotificationObserver : public NotificationObserver {
 public:
   GeolocationNotificationObserver() : infobar_(NULL) {
  }

  void ObserveInfobarAddedNotification() {
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
                   NotificationService::AllSources());
  }

  void ObserveInfobarRemovedNotification() {
    registrar_.Add(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                   NotificationService::AllSources());
  }

  void ObserveDOMOperationResponseNotification() {
    registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                   NotificationService::AllSources());
  }

  void ExecuteJavaScript(RenderViewHost* render_view_host,
                         const std::wstring& iframe_xpath,
                         const std::string& original_script) {
    LOG(WARNING) << "will run " << original_script;
    ObserveDOMOperationResponseNotification();
    std::string script = StringPrintf(
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send(%s);", original_script.c_str());
    render_view_host->ExecuteJavascriptInWebFrame(iframe_xpath,
                                                  UTF8ToWide(script));
    ui_test_utils::RunMessageLoop();
    LOG(WARNING) << "ran " << original_script;
  }

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type.value == NotificationType::TAB_CONTENTS_INFOBAR_ADDED) {
      infobar_ = Details<InfoBarDelegate>(details).ptr();
      ASSERT_TRUE(infobar_->GetIcon());
      ASSERT_TRUE(infobar_->AsConfirmInfoBarDelegate());
      MessageLoopForUI::current()->Quit();
    } else if (type.value == NotificationType::TAB_CONTENTS_INFOBAR_REMOVED) {
      infobar_ = NULL;
    } else if (type == NotificationType::DOM_OPERATION_RESPONSE) {
      Details<DomOperationNotificationDetails> dom_op_details(details);
      javascript_response_ = dom_op_details->json();
      LOG(WARNING) << "javascript_response " << javascript_response_;
      MessageLoopForUI::current()->Quit();
    }
  }

  NotificationRegistrar registrar_;
  InfoBarDelegate* infobar_;
  std::string javascript_response_;
};

void NotifyGeopositionOnIOThread(const Geoposition& geoposition) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  DCHECK(MockLocationProvider::instance_);
  MockLocationProvider::instance_->position_ = geoposition;
  MockLocationProvider::instance_->UpdateListeners();
  LOG(WARNING) << "MockLocationProvider listeners updated";
}

// This is a browser test for Geolocation.
// It exercises various integration points from javascript <-> browser:
// 1. Infobar is displayed when a geolocation is requested from an unauthorized
// origin.
// 2. Denying the infobar triggers the correct error callback.
// 3. Allowing the infobar does not trigger an error, and allow a geoposition to
// be passed to javascript.
// 4. Permissions persisted in disk are respected.
// 5. Off the record profiles don't use saved permissions.
class GeolocationBrowserTest : public InProcessBrowserTest {
 public:
  GeolocationBrowserTest()
    : infobar_(NULL), current_browser_(NULL),
      html_for_tests_("files/geolocation/simple.html") {
    EnableDOMAutomation();
  }

  enum InitializationOptions {
    INITIALIZATION_NONE,
    INITIALIZATION_OFFTHERECORD,
    INITIALIZATION_NEWTAB,
    INITIALIZATION_IFRAMES,
  };

  void Initialize(InitializationOptions options) {
    GeolocationArbitrator::SetProviderFactoryForTest(
        &NewAutoSuccessMockLocationProvider);
    if (!server_.get())
      server_ = StartHTTPServer();

    current_url_ = server_->TestServerPage(html_for_tests_);
    LOG(WARNING) << "before navigate";
    if (options == INITIALIZATION_OFFTHERECORD) {
      ui_test_utils::OpenURLOffTheRecord(browser()->profile(), current_url_);
      current_browser_ = BrowserList::FindBrowserWithType(
          browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL);
    } else if (options == INITIALIZATION_NEWTAB) {
      current_browser_ = browser();
      current_browser_->NewTab();
      ui_test_utils::NavigateToURL(current_browser_, current_url_);
    } else if (options == INITIALIZATION_IFRAMES) {
      current_browser_ = browser();
      ui_test_utils::NavigateToURL(current_browser_, current_url_);
      IFrameLoader iframe0(current_browser_, 0);
      iframe0_url_ = iframe0.iframe_url();

      IFrameLoader iframe1(current_browser_, 1);
      iframe1_url_ = iframe1.iframe_url();
    } else {
      current_browser_ = browser();
      ui_test_utils::NavigateToURL(current_browser_, current_url_);
    }
    EXPECT_TRUE(current_browser_);
    LOG(WARNING) << "after navigate";
  }

  void AddGeolocationWatch(bool wait_for_infobar) {
    GeolocationNotificationObserver notification_observer;
    if (wait_for_infobar) {
      // Observe infobar notification.
      notification_observer.ObserveInfobarAddedNotification();
    }

    notification_observer.ExecuteJavaScript(
        current_browser_->GetSelectedTabContents()->render_view_host(),
        iframe_xpath_, "geoStart()");
    EXPECT_NE("0", notification_observer.javascript_response_);
    LOG(WARNING) << "got geolocation watch";

    if (wait_for_infobar) {
      if (!notification_observer.infobar_) {
        LOG(WARNING) << "will block for infobar";
        ui_test_utils::RunMessageLoop();
        LOG(WARNING) << "infobar created";
      }
      EXPECT_TRUE(notification_observer.infobar_);
      infobar_ = notification_observer.infobar_;
    }
  }

  Geoposition GeopositionFromLatLong(double latitude, double longitude) {
    Geoposition geoposition;
    geoposition.latitude = latitude;
    geoposition.longitude = longitude;
    geoposition.accuracy = 0;
    geoposition.error_code = Geoposition::ERROR_CODE_NONE;
    // Webkit compares the timestamp to wall clock time, so we need
    // it to be contemporary.
    geoposition.timestamp = base::Time::Now();
    EXPECT_TRUE(geoposition.IsValidFix());
    return geoposition;
  }

  void CheckGeoposition(const Geoposition& geoposition) {
    // Checks we have no error.
    CheckStringValueFromJavascript("0", "geoGetLastError()");
    CheckStringValueFromJavascript(
        DoubleToString(geoposition.latitude), "geoGetLastPositionLatitude()");
    CheckStringValueFromJavascript(
        DoubleToString(geoposition.longitude), "geoGetLastPositionLongitude()");
  }

  void SetInfobarResponse(const GURL& requesting_url, bool allowed) {
    TabContents* tab_contents = current_browser_->GetSelectedTabContents();
    TabContents::GeolocationContentSettings content_settings =
        tab_contents->geolocation_content_settings();
    size_t settings_size = content_settings.size();
    GeolocationNotificationObserver notification_observer;
    notification_observer.ObserveInfobarRemovedNotification();
    ASSERT_TRUE(infobar_);
    LOG(WARNING) << "will set infobar response";
    if (allowed)
      infobar_->AsConfirmInfoBarDelegate()->Accept();
    else
      infobar_->AsConfirmInfoBarDelegate()->Cancel();
    LOG(WARNING) << "infobar response set";
    EXPECT_FALSE(notification_observer.infobar_);
    infobar_ = NULL;
    WaitForJSPrompt();
    content_settings = tab_contents->geolocation_content_settings();
    EXPECT_GT(content_settings.size(), settings_size);
    EXPECT_EQ(1U, content_settings.count(requesting_url));
    ContentSetting expected_setting =
          allowed ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
    EXPECT_EQ(expected_setting, content_settings[requesting_url]);
  }

  void WaitForJSPrompt() {
    LOG(WARNING) << "will block for JS prompt";
    AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
    LOG(WARNING) << "JS prompt received";
    ASSERT_TRUE(alert);
    LOG(WARNING) << "will close JS prompt";
    alert->CloseModalDialog();
    LOG(WARNING) << "closed JS prompt";
  }

  void CheckStringValueFromJavascript(
      const std::string& expected, const std::string& function) {
    GeolocationNotificationObserver notification_observer;
    notification_observer.ExecuteJavaScript(
        current_browser_->GetSelectedTabContents()->render_view_host(),
        iframe_xpath_, function);
    EXPECT_EQ(StringPrintf("\"%s\"", expected.c_str()),
              notification_observer.javascript_response_);
  }

  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableGeolocation);
  }

  scoped_refptr<HTTPTestServer> server_;
  InfoBarDelegate* infobar_;
  Browser* current_browser_;
  // path element of a URL referencing the html content for this test.
  std::string html_for_tests_;
  // This member defines the iframe (or top-level page, if empty) where the
  // javascript calls will run.
  std::wstring iframe_xpath_;
  // The current url for the top level page.
  GURL current_url_;
  // If not empty, the GURL for the first iframe.
  GURL iframe0_url_;
  // If not empty, the GURL for the second iframe.
  GURL iframe1_url_;
};

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_DisplaysPermissionBar DISABLED_DisplaysPermissionBar
#else
#define MAYBE_DisplaysPermissionBar DisplaysPermissionBar
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_DisplaysPermissionBar) {
  Initialize(INITIALIZATION_NONE);
  AddGeolocationWatch(true);
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_Geoposition DISABLED_Geoposition
#else
#define MAYBE_Geoposition Geoposition
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_Geoposition) {
  Initialize(INITIALIZATION_NONE);
  AddGeolocationWatch(true);
  SetInfobarResponse(current_url_, true);
  CheckGeoposition(MockLocationProvider::instance_->position_);
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_ErrorOnPermissionDenied DISABLED_ErrorOnPermissionDenied
#else
#define MAYBE_ErrorOnPermissionDenied ErrorOnPermissionDenied
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_ErrorOnPermissionDenied) {
  Initialize(INITIALIZATION_NONE);
  AddGeolocationWatch(true);
  // Infobar was displayed, deny access and check for error code.
  SetInfobarResponse(current_url_, false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_NoInfobarForSecondTab DISABLED_NoInfobarForSecondTab
#else
#define MAYBE_NoInfobarForSecondTab NoInfobarForSecondTab
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoInfobarForSecondTab) {
#if 0
  // TODO(bulach): enable this test once we use HostContentSettingsMap instead
  // of files.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, GeopositionFromLatLong(0, 0));
  SetInfobarResponse(current_url_, true);
  // Checks infobar will not be created a second tab.
  Initialize(INITIALIZATION_NEWTAB);
  SendGeoposition(false, GeopositionFromLatLong(0, 0));
  CheckStringValueFromJavascript("0", "geoGetLastError()");
#endif
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_NoInfobarForDeniedOrigin DISABLED_NoInfobarForDeniedOrigin
#else
#define MAYBE_NoInfobarForDeniedOrigin NoInfobarForDeniedOrigin
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_NoInfobarForDeniedOrigin) {
#if 0
  // TODO(bulach): enable this test once we use HostContentSettingsMap instead
  // of files.
  WritePermissionFile("{\"allowed\":false}");
  // Checks no infobar will be created.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(false, GeopositionFromLatLong(0, 0));
  CheckStringValueFromJavascript("1", "geoGetLastError()");
  // Checks infobar will not be created a second tab.
  Initialize(INITIALIZATION_NEWTAB);
  SendGeoposition(false, GeopositionFromLatLong(0, 0));
  CheckStringValueFromJavascript("1", "geoGetLastError()");
#endif
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_NoInfobarForAllowedOrigin DISABLED_NoInfobarForAllowedOrigin
#else
#define MAYBE_NoInfobarForAllowedOrigin NoInfobarForAllowedOrigin
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       MAYBE_NoInfobarForAllowedOrigin) {
#if 0
  // TODO(bulach): enable this test once we use HostContentSettingsMap instead
  // of files.
  WritePermissionFile("{\"allowed\":true}");
  // Checks no infobar will be created and there's no error callback.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(false, GeopositionFromLatLong(0, 0));
  CheckStringValueFromJavascript("0", "geoGetLastError()");
#endif
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_InfobarForOffTheRecord DISABLED_InfobarForOffTheRecord
#else
#define MAYBE_InfobarForOffTheRecord InfobarForOffTheRecord
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_InfobarForOffTheRecord) {
  // Checks infobar will be created for regular profile.
  Initialize(INITIALIZATION_NONE);
  AddGeolocationWatch(true);
  SetInfobarResponse(current_url_, true);
  CheckGeoposition(MockLocationProvider::instance_->position_);
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("false", "geoEnableAlerts(false)");
  // Go off the record, and checks infobar will be created and an error callback
  // is triggered.
  Initialize(INITIALIZATION_OFFTHERECORD);
  AddGeolocationWatch(true);
  SetInfobarResponse(current_url_, false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_IFramesWithFreshPosition DISABLED_IFramesWithFreshPosition
#else
// TODO(bulach): investigate this failure.
// http://build.chromium.org/buildbot/waterfall/builders/XP%20Tests/builds/18549/steps/browser_tests/logs/stdio
#define MAYBE_IFramesWithFreshPosition FLAKY_IFramesWithFreshPosition
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       MAYBE_IFramesWithFreshPosition) {
  html_for_tests_ = "files/geolocation/iframes_different_origin.html";
  Initialize(INITIALIZATION_IFRAMES);
  LOG(WARNING) << "frames loaded";

  iframe_xpath_ = L"//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe0_url_, true);
  CheckGeoposition(MockLocationProvider::instance_->position_);
  // Disables further prompts from this iframe.
  CheckStringValueFromJavascript("false", "geoEnableAlerts(false)");

  // Test second iframe from a different origin with a cached geoposition will
  // create the infobar.
  iframe_xpath_ = L"//iframe[@id='iframe_1']";
  AddGeolocationWatch(true);

  // Back to the first frame, enable alert and refresh geoposition.
  iframe_xpath_ = L"//iframe[@id='iframe_0']";
  CheckStringValueFromJavascript("true", "geoEnableAlerts(true)");
  // MockLocationProvider must have been created.
  ASSERT_TRUE(MockLocationProvider::instance_);
  Geoposition fresh_position = GeopositionFromLatLong(3.17, 4.23);
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableFunction(
      &NotifyGeopositionOnIOThread, fresh_position));
  WaitForJSPrompt();
  CheckGeoposition(fresh_position);

  // Disable alert for this frame.
  CheckStringValueFromJavascript("false", "geoEnableAlerts(false)");

  // Now go ahead an authorize the second frame.
  iframe_xpath_ = L"//iframe[@id='iframe_1']";
  // Infobar was displayed, allow access and check there's no error code.
  SetInfobarResponse(iframe1_url_, true);
  CheckGeoposition(fresh_position);
}


#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_IFramesWithCachedPosition DISABLED_IFramesWithCachedPosition
#else
// TODO(bulach): enable this test when we roll to
// https://bugs.webkit.org/show_bug.cgi?id=36315
#define MAYBE_IFramesWithCachedPosition DISABLED_IFramesWithCachedPosition
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest,
                       MAYBE_IFramesWithCachedPosition) {
  html_for_tests_ = "files/geolocation/iframes_different_origin.html";
  Initialize(INITIALIZATION_IFRAMES);

  iframe_xpath_ = L"//iframe[@id='iframe_0']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe0_url_, true);
  CheckGeoposition(MockLocationProvider::instance_->position_);

  // Refresh geoposition, but let's not yet create the watch on the second frame
  // so that it'll fetch from cache.
  // MockLocationProvider must have been created.
  ASSERT_TRUE(MockLocationProvider::instance_);
  Geoposition cached_position = GeopositionFromLatLong(3.17, 4.23);
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableFunction(
      &NotifyGeopositionOnIOThread, cached_position));
  WaitForJSPrompt();
  CheckGeoposition(cached_position);

  // Disable alert for this frame.
  CheckStringValueFromJavascript("false", "geoEnableAlerts(false)");

  // Now go ahead an authorize the second frame.
  iframe_xpath_ = L"//iframe[@id='iframe_1']";
  AddGeolocationWatch(true);
  SetInfobarResponse(iframe1_url_, true);
  CheckGeoposition(cached_position);
}
