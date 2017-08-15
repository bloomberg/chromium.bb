// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_request_manager.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/custom_handlers/register_protocol_handler_permission_request.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/webrtc/media_stream_devices_controller.h"
#include "chrome/browser/permissions/permission_context_base.h"
#include "chrome/browser/permissions/permission_request_impl.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/permission_bubble/mock_permission_prompt_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace test {
class MediaStreamDevicesControllerTestApi
    : public MediaStreamDevicesController::PermissionPromptDelegate {
 public:
  static void AddRequestToManager(
      PermissionRequestManager* manager,
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) {
    MediaStreamDevicesControllerTestApi delegate(manager);
    MediaStreamDevicesController::RequestPermissionsWithDelegate(
        request, callback, &delegate);
  }

 private:
  // MediaStreamDevicesController::PermissionPromptDelegate:
  void ShowPrompt(
      bool user_gesture,
      content::WebContents* web_contents,
      std::unique_ptr<MediaStreamDevicesController::Request> request) override {
    manager_->AddRequest(request.release());
  }

  explicit MediaStreamDevicesControllerTestApi(
      PermissionRequestManager* manager)
      : manager_(manager) {}

  PermissionRequestManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamDevicesControllerTestApi);
};
}  // namespace test

namespace {

const char* kPermissionsKillSwitchFieldStudy =
    PermissionContextBase::kPermissionsKillSwitchFieldStudy;
const char* kPermissionsKillSwitchBlockedValue =
    PermissionContextBase::kPermissionsKillSwitchBlockedValue;
const char kPermissionsKillSwitchTestGroup[] = "TestGroup";

class PermissionRequestManagerBrowserTest : public InProcessBrowserTest {
 public:
  PermissionRequestManagerBrowserTest() = default;
  ~PermissionRequestManagerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PermissionRequestManager* manager = GetPermissionRequestManager();
    mock_permission_prompt_factory_.reset(
        new MockPermissionPromptFactory(manager));
    manager->DisplayPendingRequests();
  }

  void TearDownOnMainThread() override {
    mock_permission_prompt_factory_.reset();
  }

  PermissionRequestManager* GetPermissionRequestManager() {
    return PermissionRequestManager::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
  }

  MockPermissionPromptFactory* bubble_factory() {
    return mock_permission_prompt_factory_.get();
  }

  void EnableKillSwitch(ContentSettingsType content_settings_type) {
    std::map<std::string, std::string> params;
    params[PermissionUtil::GetPermissionString(content_settings_type)] =
        kPermissionsKillSwitchBlockedValue;
    variations::AssociateVariationParams(
        kPermissionsKillSwitchFieldStudy, kPermissionsKillSwitchTestGroup,
        params);
    base::FieldTrialList::CreateFieldTrial(kPermissionsKillSwitchFieldStudy,
                                           kPermissionsKillSwitchTestGroup);
  }

 private:
  std::unique_ptr<MockPermissionPromptFactory> mock_permission_prompt_factory_;

  DISALLOW_COPY_AND_ASSIGN(PermissionRequestManagerBrowserTest);
};

// Harness for testing permissions dialogs invoked by PermissionRequestManager.
// Uses a "real" PermissionPromptFactory rather than a mock.
class PermissionDialogTest
    : public SupportsTestDialog<PermissionRequestManagerBrowserTest> {
 public:
  PermissionDialogTest() {}

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    // Skip super: It will install a mock permission UI factory, but for this
    // test we want to show "real" UI.
    ui_test_utils::NavigateToURL(browser(), GetUrl());
  }

 private:
  GURL GetUrl() { return GURL("https://example.com"); }

  PermissionRequest* MakeRegisterProtocolHandlerRequest();
  void AddMediaRequest(PermissionRequestManager* manager,
                       ContentSettingsType permission);
  PermissionRequest* MakePermissionRequest(ContentSettingsType permission);

  // TestBrowserDialog:
  void ShowDialog(const std::string& name) override;

  // Holds requests that do not delete themselves.
  std::vector<std::unique_ptr<PermissionRequest>> owned_requests_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDialogTest);
};

PermissionRequest* PermissionDialogTest::MakeRegisterProtocolHandlerRequest() {
  std::string protocol = "mailto";
  bool user_gesture = true;
  ProtocolHandler handler =
      ProtocolHandler::CreateProtocolHandler(protocol, GetUrl());
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(
          browser()->profile());
  // Deleted in RegisterProtocolHandlerPermissionRequest::RequestFinished().
  return new RegisterProtocolHandlerPermissionRequest(registry, handler,
                                                      GetUrl(), user_gesture);
}

void PermissionDialogTest::AddMediaRequest(PermissionRequestManager* manager,
                                           ContentSettingsType permission) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::MediaStreamRequestType request_type = content::MEDIA_DEVICE_ACCESS;
  content::MediaStreamType audio_type = content::MEDIA_NO_SERVICE;
  content::MediaStreamType video_type = content::MEDIA_NO_SERVICE;
  std::string audio_id = "audio_id";
  std::string video_id = "video_id";

  if (permission == CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC)
    audio_type = content::MEDIA_DEVICE_AUDIO_CAPTURE;
  else
    video_type = content::MEDIA_DEVICE_VIDEO_CAPTURE;
  int render_process_id = web_contents->GetRenderProcessHost()->GetID();
  int render_frame_id = web_contents->GetMainFrame()->GetRoutingID();
  content::MediaStreamRequest request(render_process_id, render_frame_id, 0,
                                      GetUrl(), false, request_type, audio_id,
                                      video_id, audio_type, video_type, false);

  // Add fake devices, otherwise the request will auto-block.
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestAudioCaptureDevices(
      content::MediaStreamDevices(
          1, content::MediaStreamDevice(content::MEDIA_DEVICE_AUDIO_CAPTURE,
                                        audio_id, "Fake Audio")));
  MediaCaptureDevicesDispatcher::GetInstance()->SetTestVideoCaptureDevices(
      content::MediaStreamDevices(
          1, content::MediaStreamDevice(content::MEDIA_DEVICE_VIDEO_CAPTURE,
                                        video_id, "Fake Video")));

  auto response = [](const content::MediaStreamDevices& devices,
                     content::MediaStreamRequestResult result,
                     std::unique_ptr<content::MediaStreamUI> ui) {};
  test::MediaStreamDevicesControllerTestApi::AddRequestToManager(
      manager, web_contents, request, base::Bind(response));
}

PermissionRequest* PermissionDialogTest::MakePermissionRequest(
    ContentSettingsType permission) {
  bool user_gesture = true;
  auto decided = [](bool, ContentSetting) {};
  auto cleanup = [] {};  // Leave cleanup to test harness destructor.
  owned_requests_.push_back(base::MakeUnique<PermissionRequestImpl>(
      GetUrl(), permission, browser()->profile(), user_gesture,
      base::Bind(decided), base::Bind(cleanup)));
  return owned_requests_.back().get();
}

void PermissionDialogTest::ShowDialog(const std::string& name) {
  constexpr const char* kMultipleName = "multiple";
  constexpr struct {
    const char* name;
    ContentSettingsType type;
  } kNameToType[] = {
      {"flash", CONTENT_SETTINGS_TYPE_PLUGINS},
      {"geolocation", CONTENT_SETTINGS_TYPE_GEOLOCATION},
      {"protected_media", CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER},
      {"notifications", CONTENT_SETTINGS_TYPE_NOTIFICATIONS},
      {"mic", CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC},
      {"camera", CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA},
      {"protocol_handlers", CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS},
      {"midi", CONTENT_SETTINGS_TYPE_MIDI_SYSEX},
      {kMultipleName, CONTENT_SETTINGS_TYPE_DEFAULT}};
  const auto* it = std::begin(kNameToType);
  for (; it != std::end(kNameToType); ++it) {
    if (name == it->name)
      break;
  }
  if (it == std::end(kNameToType)) {
    ADD_FAILURE() << "Unknown: " << name;
    return;
  }
  PermissionRequestManager* manager = GetPermissionRequestManager();
  switch (it->type) {
    case CONTENT_SETTINGS_TYPE_PROTOCOL_HANDLERS:
      manager->AddRequest(MakeRegisterProtocolHandlerRequest());
      break;
    case CONTENT_SETTINGS_TYPE_AUTOMATIC_DOWNLOADS:
      // TODO(tapted): Prompt for downloading multiple files.
      break;
    case CONTENT_SETTINGS_TYPE_DURABLE_STORAGE:
      // TODO(tapted): Prompt for quota request.
      break;
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC:
    case CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA:
      if (base::FeatureList::IsEnabled(
              features::kUsePermissionManagerForMediaRequests)) {
        manager->AddRequest(MakePermissionRequest(it->type));
      } else {
        AddMediaRequest(manager, it->type);
      }
      break;
    // Regular permissions requests.
    case CONTENT_SETTINGS_TYPE_MIDI_SYSEX:
    case CONTENT_SETTINGS_TYPE_PUSH_MESSAGING:  // Same as notifications.
    case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
    case CONTENT_SETTINGS_TYPE_GEOLOCATION:
    case CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER:  // ChromeOS only.
    case CONTENT_SETTINGS_TYPE_PPAPI_BROKER:
    case CONTENT_SETTINGS_TYPE_PLUGINS:  // Flash.
      manager->AddRequest(MakePermissionRequest(it->type));
      break;
    case CONTENT_SETTINGS_TYPE_DEFAULT:
      // Permissions to request for a "multiple" request. Only mic/camera
      // requests are grouped together.
      EXPECT_EQ(kMultipleName, name);
      manager->AddRequest(
          MakePermissionRequest(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC));
      manager->AddRequest(
          MakePermissionRequest(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA));

      break;
    default:
      ADD_FAILURE() << "Not a permission type, or one that doesn't prompt.";
      return;
  }
  manager->DisplayPendingRequests();
}

// Requests before the load event should be bundled into one bubble.
// http://crbug.com/512849 flaky
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       DISABLED_RequestsBeforeLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->TotalRequestCount());
}

// Requests before the load should not be bundled with a request after the load.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       RequestsBeforeAfterLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-after-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Navigating twice to the same URL should be equivalent to refresh. This means
// showing the bubbles twice.
// http://crbug.com/512849 flaky
#if defined(OS_WIN)
#define MAYBE_NavTwice DISABLED_NavTwice
#else
#define MAYBE_NavTwice NavTwice
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest, MAYBE_NavTwice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(2, bubble_factory()->show_count());
  EXPECT_EQ(2, bubble_factory()->TotalRequestCount());
}

// Navigating twice to the same URL with a hash should be navigation within the
// page. This means the bubble is only shown once.
// http://crbug.com/512849 flaky
#if defined(OS_WIN)
#define MAYBE_NavTwiceWithHash DISABLED_NavTwiceWithHash
#else
#define MAYBE_NavTwiceWithHash NavTwiceWithHash
#endif
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       MAYBE_NavTwiceWithHash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/permissions/requests-before-load.html"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL(
          "/permissions/requests-before-load.html#0"),
      1);
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Bubble requests should be shown after in-page navigation.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest, InPageNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/empty.html"),
      1);

  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(),
      embedded_test_server()->GetURL("/empty.html#0"),
      1);

  // Request 'geolocation' permission.
  ExecuteScriptAndGetValue(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      "navigator.geolocation.getCurrentPosition(function(){});");
  bubble_factory()->WaitForPermissionBubble();

  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());
}

// Bubble requests should not be shown when the killswitch is on.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       KillSwitchGeolocation) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "requestGeolocation();"));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Now enable the geolocation killswitch.
  EnableKillSwitch(CONTENT_SETTINGS_TYPE_GEOLOCATION);

  // Reload the page to get around blink layer caching for geolocation
  // requests.
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html"));

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "requestGeolocation();", &result));
  EXPECT_EQ("denied", result);
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  variations::testing::ClearAllVariationParams();
}

// Bubble requests should not be shown when the killswitch is on.
IN_PROC_BROWSER_TEST_F(PermissionRequestManagerBrowserTest,
                       KillSwitchNotifications) {
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/permissions/killswitch_tester.html"));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::ExecuteScript(web_contents, "requestNotification();"));
  bubble_factory()->WaitForPermissionBubble();
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  // Now enable the notifications killswitch.
  EnableKillSwitch(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, "requestNotification();", &result));
  EXPECT_EQ("denied", result);
  EXPECT_EQ(1, bubble_factory()->show_count());
  EXPECT_EQ(1, bubble_factory()->TotalRequestCount());

  variations::testing::ClearAllVariationParams();

}

// Host wants to run flash.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_flash) {
  RunDialog();
}

// Host wants to know your location.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_geolocation) {
  RunDialog();
}

// Host wants to show notifications.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_notifications) {
  RunDialog();
}

// Host wants to use your microphone.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_mic) {
  RunDialog();

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kUsePermissionManagerForMediaRequests);
    RunDialog();
  }
}

// Host wants to use your camera.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_camera) {
  RunDialog();

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        features::kUsePermissionManagerForMediaRequests);
    RunDialog();
  }
}

// Host wants to open email links.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_protocol_handlers) {
  RunDialog();
}

// Host wants to use your MIDI devices.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_midi) {
  RunDialog();
}

// Shows a permissions bubble with multiple requests.
IN_PROC_BROWSER_TEST_F(PermissionDialogTest, InvokeDialog_multiple) {
  RunDialog();
}

// CONTENT_SETTINGS_TYPE_PROTECTED_MEDIA_IDENTIFIER is ChromeOS only.
#if defined(OS_CHROMEOS)
#define MAYBE_InvokeDialog_protected_media InvokeDialog_protected_media
#else
#define MAYBE_InvokeDialog_protected_media DISABLED_InvokeDialog_protected_media
#endif
IN_PROC_BROWSER_TEST_F(PermissionDialogTest,
                       MAYBE_InvokeDialog_protected_media) {
  RunDialog();
}

}  // anonymous namespace
