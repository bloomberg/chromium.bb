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
                         const std::string& original_script) {
    LOG(WARNING) << "will run " << original_script;
    ObserveDOMOperationResponseNotification();
    std::string script = StringPrintf(
        "window.domAutomationController.setAutomationId(0);"
        "window.domAutomationController.send(%s);", original_script.c_str());
    render_view_host->ExecuteJavascriptInWebFrame(L"", UTF8ToWide(script));
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

// This class is used so that we can block the UI thread until
// LocationArbitrator is fully setup and has acquired the MockLocationProvider.
// This setup occurs on the IO thread within a message loop that is running a
// javascript call. We can't simply MessageLoopForUI::current()->Quit()
// otherwise we won't be able to fetch the javascript result.
class MockLocationProviderSetup {
 public:
   MockLocationProviderSetup() : mock_transfer_event_(true, false) {
     DCHECK(!instance_);
     instance_ = this;
     GeolocationArbitrator::SetProviderFactoryForTest(&MockCreator);
   }

   ~MockLocationProviderSetup() {
     DCHECK(instance_ == this);
     instance_ = NULL;
     GeolocationArbitrator::SetProviderFactoryForTest(NULL);
   }

   void BlockForArbitratorSetup() {
     LOG(WARNING) << "will block for arbitrator setup";
     mock_transfer_event_.Wait();
     EXPECT_TRUE(MockLocationProvider::instance_);
     LOG(WARNING) << "arbitrator setup";
   }

   static LocationProviderBase* MockCreator() {
     LOG(WARNING) << "will create mock";
     LocationProviderBase* mock = NewMockLocationProvider();
     instance_->mock_transfer_event_.Signal();
     return mock;
   }

   static MockLocationProviderSetup* instance_;
   base::WaitableEvent mock_transfer_event_;
};

MockLocationProviderSetup* MockLocationProviderSetup::instance_ = NULL;

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
    if (!server_.get())
      server_ = StartHTTPServer();
    if (!mock_location_provider_setup_.get())
      mock_location_provider_setup_.reset(new MockLocationProviderSetup);

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
  }

  void AddGeolocationWatch(bool wait_for_infobar) {
    GeolocationNotificationObserver notification_observer;
    if (wait_for_infobar) {
      // Observe infobar notification.
      notification_observer.ObserveInfobarAddedNotification();
    }

    notification_observer.ExecuteJavaScript(
        current_browser_->GetSelectedTabContents()->render_view_host(),
        "geoStart()");
    EXPECT_NE("0", notification_observer.javascript_response_);
    LOG(WARNING) << "got geolocation watch";

    mock_location_provider_setup_->BlockForArbitratorSetup();

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
    geoposition.timestamp = base::Time::FromDoubleT(0.0);
    EXPECT_TRUE(geoposition.IsValidFix());
    return geoposition;
  }

  void SendGeoposition(bool wait_for_infobar, const Geoposition& geoposition) {
    GeolocationNotificationObserver notification_observer;
    if (wait_for_infobar) {
      // Observe infobar notification.
      notification_observer.ObserveInfobarAddedNotification();
    }

    // Sending the Geoposition makes webkit trigger the permission request flow.
    // If the origin is already allowed, no infobar will be displayed.
    LOG(WARNING) << "will update geoposition";

    // MockLocationProvider must have been created.
    DCHECK(MockLocationProvider::instance_);
    ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, NewRunnableFunction(
        &NotifyGeopositionOnIOThread, geoposition));
    LOG(WARNING) << "geoposition updated";

    if (wait_for_infobar) {
      LOG(WARNING) << "will block for infobar";
      ui_test_utils::RunMessageLoop();
      LOG(WARNING) << "infobar created";
      EXPECT_TRUE(notification_observer.infobar_);
      infobar_ = notification_observer.infobar_;
    } else {
      WaitForJSPrompt();
    }
  }

  void SetInfobarResponse(bool allowed) {
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
        function);
    EXPECT_EQ(StringPrintf("\"%s\"", expected.c_str()),
              notification_observer.javascript_response_);
  }

  // InProcessBrowserTest
  virtual void SetUpCommandLine(CommandLine* command_line) {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableGeolocation);
  }

  scoped_refptr<HTTPTestServer> server_;
  scoped_ptr<MockLocationProviderSetup> mock_location_provider_setup_;
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
  AddGeolocationWatch(false);
  SendGeoposition(true, GeopositionFromLatLong(0, 0));
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
  AddGeolocationWatch(false);
  SendGeoposition(true, GeopositionFromLatLong(0, 0));
  // Infobar was displayed, deny access and check for error code.
  SetInfobarResponse(false);
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
  SetInfobarResponse(true);
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
  AddGeolocationWatch(false);
  SendGeoposition(true, GeopositionFromLatLong(0, 0));
  SetInfobarResponse(true);
  CheckStringValueFromJavascript("0", "geoGetLastError()");
  // Disables further prompts from this tab.
  CheckStringValueFromJavascript("false", "geoDisableAlerts()");
  // Go off the record, and checks infobar will be created and an error callback
  // is triggered.
  Initialize(INITIALIZATION_OFFTHERECORD);
  AddGeolocationWatch(true);
  SetInfobarResponse(false);
  CheckStringValueFromJavascript("1", "geoGetLastError()");
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
  AddGeolocationWatch(false);
  SendGeoposition(true, GeopositionFromLatLong(0, 0));
  // Infobar was displayed, allow access and check there's no error code.
  SetInfobarResponse(true);
  CheckStringValueFromJavascript("0", "geoGetLastError()");
  // Sends a Geoposition over IPC, and check it arrives in the javascript side.
  Geoposition geoposition = GeopositionFromLatLong(3.17, 4.23);
  SendGeoposition(false, geoposition);
  // Checks we have no error.
  CheckStringValueFromJavascript("0", "geoGetLastError()");
  CheckStringValueFromJavascript(
      DoubleToString(geoposition.latitude), "geoGetLastPositionLatitude()");
  CheckStringValueFromJavascript(
      DoubleToString(geoposition.longitude), "geoGetLastPositionLongitude()");
}
