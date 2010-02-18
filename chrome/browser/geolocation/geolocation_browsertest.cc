// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_list.h"
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

// This is a browser test for Geolocation.
// It exercises various integration points from javascript <-> browser:
// 1. Infobar is displayed when a geolocation is requested from an unauthorized
// origin.
// 2. Denying the infobar triggers the correct error callback.
// 3. Allowing the infobar does not trigger an error, and allow a geoposition to
// be passed to javascript.
// 4. Permissions persisted in disk are respected.
// 5. Off the record profiles don't use saved permissions.
class GeolocationBrowserTest
    : public InProcessBrowserTest, public NotificationObserver {
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
    if (!server_.get()) {
      server_ = StartHTTPServer();
    }
    GURL url = server_->TestServerPage("files/geolocation/simple.html");
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

    int watch_id = 0;
    EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractInt(
        current_browser_->GetSelectedTabContents()->render_view_host(), L"",
        UTF8ToWide("window.domAutomationController.send(geoStart());"),
        &watch_id));
    EXPECT_GT(watch_id, 0);
  }

  void SendGeoposition(bool wait_for_infobar, const Geoposition& geoposition) {
    if (wait_for_infobar) {
      // Observe infobar notification.
      registrar_.Add(
          this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
          NotificationService::AllSources());
    }

    // Sending the Geoposition makes webkit trigger the permission request flow.
    // If the origin is already allowed, no infobar will be displayed.
    RenderViewHost* render_view_host =
        current_browser_->GetSelectedTabContents()->render_view_host();
    render_view_host->Send(
        new ViewMsg_Geolocation_PositionUpdated(
            render_view_host->routing_id(), geoposition));

    if (wait_for_infobar) {
      ui_test_utils::RunMessageLoop();
      EXPECT_TRUE(infobar_);
      registrar_.Remove(this, NotificationType::TAB_CONTENTS_INFOBAR_ADDED,
                        NotificationService::AllSources());
    }
  }

  void WaitForJSPrompt() {
    AppModalDialog* alert = ui_test_utils::WaitForAppModalDialog();
    ASSERT_TRUE(alert);
    alert->CloseModalDialog();
  }

  void SetInfobarResponse(bool allowed) {
    registrar_.Add(
        this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
        NotificationService::AllSources());
    ASSERT_TRUE(infobar_);
    if (allowed)
      infobar_->AsConfirmInfoBarDelegate()->Accept();
    else
      infobar_->AsConfirmInfoBarDelegate()->Cancel();
    current_browser_->GetSelectedTabContents()->RemoveInfoBar(infobar_);
    EXPECT_FALSE(infobar_);
    registrar_.Remove(this, NotificationType::TAB_CONTENTS_INFOBAR_REMOVED,
                      NotificationService::AllSources());
  }

  void CheckValueFromJavascript(
      const std::string& expected, const std::string& function) {
    std::string js_call = StringPrintf(
        "window.domAutomationController.send(%s);", function.c_str());
    std::string value;
    EXPECT_TRUE(ui_test_utils::ExecuteJavaScriptAndExtractString(
        current_browser_->GetSelectedTabContents()->render_view_host(), L"",
        UTF8ToWide(js_call),
        &value));
    EXPECT_EQ(expected, value);
  }

  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableGeolocation);
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
  Browser* current_browser_;
  scoped_refptr<HTTPTestServer> server_;
};

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, DisplaysPermissionBar) {
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, Geoposition());
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, ErrorOnPermissionDenied) {
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, Geoposition());
  // Infobar was displayed, deny access and check for error code.
  SetInfobarResponse(false);
  WaitForJSPrompt();
  CheckValueFromJavascript("1", "geoGetLastError()");
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForSecondTab) {
#if 0
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, Geoposition());
  SetInfobarResponse(true);
  WaitForJSPrompt();
  // Checks infobar will not be created a second tab.
  Initialize(INITIALIZATION_NEWTAB);
  SendGeoposition(false, Geoposition());
  WaitForJSPrompt();
  CheckValueFromJavascript("0", "geoGetLastError()");
#endif
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForDeniedOrigin) {
#if 0
  WritePermissionFile("{\"allowed\":false}");
  // Checks no infobar will be created.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(false, Geoposition());
  WaitForJSPrompt();
  CheckValueFromJavascript("1", "geoGetLastError()");
  // Checks infobar will not be created a second tab.
  Initialize(INITIALIZATION_NEWTAB);
  SendGeoposition(false, Geoposition());
  WaitForJSPrompt();
  CheckValueFromJavascript("1", "geoGetLastError()");
#endif
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, NoInfobarForAllowedOrigin) {
#if 0
  WritePermissionFile("{\"allowed\":true}");
  // Checks no infobar will be created and there's no error callback.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(false, Geoposition());
  WaitForJSPrompt();
  CheckValueFromJavascript("0", "geoGetLastError()");
#endif
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, InfobarForOffTheRecord) {
#if 0
  // Checks infobar will be created for regular profile.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, Geoposition());
  SetInfobarResponse(true);
  WaitForJSPrompt();
  CheckValueFromJavascript("0", "geoGetLastError()");
  // Go off the record, and checks infobar will be created and an error callback
  // is triggered.
  Initialize(INITIALIZATION_OFFTHERECORD);
  SendGeoposition(true, Geoposition());
  SetInfobarResponse(false);
  WaitForJSPrompt();
  CheckValueFromJavascript("1", "geoGetLastError()");
#endif
}

IN_PROC_BROWSER_TEST_F(GeolocationBrowserTest, Geoposition) {
  // Checks infobar will be created.
  Initialize(INITIALIZATION_NONE);
  SendGeoposition(true, Geoposition());
  // Infobar was displayed, allow access and check there's no error code.
  SetInfobarResponse(true);
  WaitForJSPrompt();
  CheckValueFromJavascript("0", "geoGetLastError()");
  // Sends a Geoposition over IPC, and check it arrives in the javascript side.
  Geoposition geoposition;
  geoposition.latitude = 3.17;
  geoposition.longitude = 4.23;
  SendGeoposition(false, geoposition);
  WaitForJSPrompt();
  // Checks we have no error.
  CheckValueFromJavascript("0", "geoGetLastError()");
  CheckValueFromJavascript(
      DoubleToString(geoposition.latitude), "geoGetLastPositionLatitude()");
  CheckValueFromJavascript(
      DoubleToString(geoposition.longitude), "geoGetLastPositionLongitude()");
}
