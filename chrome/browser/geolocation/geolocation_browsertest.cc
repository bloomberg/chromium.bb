// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/geolocation/location_arbitrator.h"
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

class InfobarNotificationObserver : public NotificationObserver {
 public:
  InfobarNotificationObserver() : infobar_(NULL) {
  }

  void Add(NotificationType notification_type) {
    registrar_.Add(this, notification_type, NotificationService::AllSources());
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
    }
  }

  NotificationRegistrar registrar_;
  InfoBarDelegate* infobar_;
};

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
  GeolocationBrowserTest() : infobar_(NULL), current_browser_(NULL) {
    EnableDOMAutomation();
  }

  enum InitializationOptions {
    INITIALIZATION_NONE,
    INITIALIZATION_OFFTHERECORD,
    INITIALIZATION_NEWTAB,
  };

  void Initialize(InitializationOptions options) {
    GeolocationArbitrator::SetProviderFactoryForTest(&NewMockLocationProvider);
    if (!server_.get()) {
      server_ = StartHTTPServer();
    }
    GURL url = server_->TestServerPage("files/geolocation/simple.html");
    LOG(WARNING) << "before navigate";
    if (options == INITIALIZATION_OFFTHERECORD) {
      ui_test_utils::OpenURLOffTheRecord(browser()->profile(), url);
      current_browser_ = BrowserList::FindBrowserWithType(
          browser()->profile()->GetOffTheRecordProfile(), Browser::TYPE_NORMAL);
    } else if (options == INITIALIZATION_NEWTAB) {
      current_browser_ = browser();
      current_browser_->NewTab();
      ui_test_utils::NavigateToURL(current_browser_, url);
    } else {
      current_browser_ = browser();
      ui_test_utils::NavigateToURL(current_browser_, url);
    }
    EXPECT_TRUE(current_browser_);
    LOG(WARNING) << "after navigate";

    int watch_id = 0;
    EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
        current_browser_->GetSelectedTabContents()->render_view_host(), L"",
        UTF8ToWide("window.domAutomationController.send(geoStart());"),
        &watch_id));
    EXPECT_GT(watch_id, 0);
    LOG(WARNING) << "got geolocation watch";
  }

  void SendGeoposition(bool wait_for_infobar, const Geoposition& geoposition) {
    InfobarNotificationObserver infobar_notification_observer;
    if (wait_for_infobar) {
      // Observe infobar notification.
      infobar_notification_observer.Add(
          NotificationType::TAB_CONTENTS_INFOBAR_ADDED);
    }

    // Sending the Geoposition makes webkit trigger the permission request flow.
    // If the origin is already allowed, no infobar will be displayed.
    LOG(WARNING) << "will send geoposition";
    RenderViewHost* render_view_host =
        current_browser_->GetSelectedTabContents()->render_view_host();
    render_view_host->Send(
        new ViewMsg_Geolocation_PositionUpdated(
            render_view_host->routing_id(), geoposition));
    LOG(WARNING) << "geoposition sent";

    if (wait_for_infobar) {
      LOG(WARNING) << "will block for infobar";
      ui_test_utils::RunMessageLoop();
      LOG(WARNING) << "infobar created";
      EXPECT_TRUE(infobar_notification_observer.infobar_);
      infobar_ = infobar_notification_observer.infobar_;
    } else {
      WaitForJSPrompt();
    }
  }

  void SetInfobarResponse(bool allowed) {
    InfobarNotificationObserver infobar_notification_observer;
    infobar_notification_observer.Add(
        NotificationType::TAB_CONTENTS_INFOBAR_REMOVED);
    ASSERT_TRUE(infobar_);
    LOG(WARNING) << "will set infobar response";
    if (allowed)
      infobar_->AsConfirmInfoBarDelegate()->Accept();
    else
      infobar_->AsConfirmInfoBarDelegate()->Cancel();
    LOG(WARNING) << "infobar response set";
    EXPECT_FALSE(infobar_notification_observer.infobar_);
    infobar_ = NULL;
    WaitForJSPrompt();
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

  void CheckValueFromJavascript(
      const std::string& expected, const std::string& function) {
    std::string js_call = StringPrintf(
        "window.domAutomationController.send(%s);", function.c_str());
    std::string value;
    LOG(WARNING) << "will check for JS value";
    EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        current_browser_->GetSelectedTabContents()->render_view_host(), L"",
        UTF8ToWide(js_call),
        &value));
    LOG(WARNING) << "JS value checked";
    EXPECT_EQ(expected, value);
  }

  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableGeolocation);
  }
  virtual void TearDownInProcessBrowserTestFixture() {
    GeolocationArbitrator::SetProviderFactoryForTest(NULL);
  }

  scoped_refptr<HTTPTestServer> server_;
  InfoBarDelegate* infobar_;
  Browser* current_browser_;
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
  SendGeoposition(true, Geoposition());
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
  SendGeoposition(true, Geoposition());
  // Infobar was displayed, deny access and check for error code.
  SetInfobarResponse(false);
  CheckValueFromJavascript("1", "geoGetLastError()");
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
  SendGeoposition(true, Geoposition());
  SetInfobarResponse(true);
  // Checks infobar will not be created a second tab.
  Initialize(INITIALIZATION_NEWTAB);
  SendGeoposition(false, Geoposition());
  CheckValueFromJavascript("0", "geoGetLastError()");
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
  SendGeoposition(false, Geoposition());
  CheckValueFromJavascript("1", "geoGetLastError()");
  // Checks infobar will not be created a second tab.
  Initialize(INITIALIZATION_NEWTAB);
  SendGeoposition(false, Geoposition());
  CheckValueFromJavascript("1", "geoGetLastError()");
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
  SendGeoposition(false, Geoposition());
  CheckValueFromJavascript("0", "geoGetLastError()");
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
  SendGeoposition(true, Geoposition());
  SetInfobarResponse(true);
  CheckValueFromJavascript("0", "geoGetLastError()");
  // Go off the record, and checks infobar will be created and an error callback
  // is triggered.
  Initialize(INITIALIZATION_OFFTHERECORD);
  SendGeoposition(true, Geoposition());
  SetInfobarResponse(false);
  CheckValueFromJavascript("1", "geoGetLastError()");
}

#if defined(OS_MACOSX)
// TODO(bulach): investigate why this fails on mac. It may be related to:
// http://crbug.com//29424
#define MAYBE_Geoposition DISABLED_Geoposition
#else
#define MAYBE_Geoposition Geoposition
#endif

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, MAYBE_Geoposition) {
  // Checks infobar will be created.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, Geoposition());
  // Infobar was displayed, allow access and check there's no error code.
  SetInfobarResponse(true);
  CheckValueFromJavascript("0", "geoGetLastError()");
  // Sends a Geoposition over IPC, and check it arrives in the javascript side.
  Geoposition geoposition;
  geoposition.latitude = 3.17;
  geoposition.longitude = 4.23;
  SendGeoposition(false, geoposition);
  // Checks we have no error.
  CheckValueFromJavascript("0", "geoGetLastError()");
  CheckValueFromJavascript(
      DoubleToString(geoposition.latitude), "geoGetLastPositionLatitude()");
  CheckValueFromJavascript(
      DoubleToString(geoposition.longitude), "geoGetLastPositionLongitude()");
}
