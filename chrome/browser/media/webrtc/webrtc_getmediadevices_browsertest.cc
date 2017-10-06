// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

const char kDeviceKindAudioInput[] = "audioinput";
const char kDeviceKindVideoInput[] = "videoinput";
const char kDeviceKindAudioOutput[] = "audiooutput";

}  // namespace

// Integration test for WebRTC enumerateDevices. It always uses fake devices.
// It needs to be a browser test (and not content browser test) to be able to
// test that labels are cleared or not depending on if access to devices has
// been granted.
class WebRtcGetMediaDevicesBrowserTest
    : public WebRtcTestBase,
      public testing::WithParamInterface<bool> {
 public:
  WebRtcGetMediaDevicesBrowserTest()
      : has_audio_output_devices_initialized_(false),
        has_audio_output_devices_(false) {}

  void SetUpInProcessBrowserTestFixture() override {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Ensure the infobar is enabled, since we expect that in this test.
    EXPECT_FALSE(command_line->HasSwitch(switches::kUseFakeUIForMediaStream));

    // Always use fake devices.
    command_line->AppendSwitch(switches::kUseFakeDeviceForMediaStream);
  }

 protected:
  // This is used for media devices and sources.
  struct MediaDeviceInfo {
    std::string device_id;  // Domain specific device ID.
    std::string kind;
    std::string label;
    std::string group_id;
  };

  void EnumerateDevices(content::WebContents* tab,
                        std::vector<MediaDeviceInfo>* devices) {
    std::string devices_as_json = ExecuteJavascript("enumerateDevices()", tab);
    EXPECT_FALSE(devices_as_json.empty());

    int error_code;
    std::string error_message;
    std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
        devices_as_json, base::JSON_ALLOW_TRAILING_COMMAS, &error_code,
        &error_message);

    ASSERT_TRUE(value.get() != NULL) << error_message;
    EXPECT_EQ(value->type(), base::Value::Type::LIST);

    base::ListValue* values;
    ASSERT_TRUE(value->GetAsList(&values));
    ASSERT_FALSE(values->empty());
    bool found_audio_input = false;
    bool found_video_input = false;

    for (base::ListValue::iterator it = values->begin();
         it != values->end(); ++it) {
      const base::DictionaryValue* dict;
      MediaDeviceInfo device;
      ASSERT_TRUE(it->GetAsDictionary(&dict));
      ASSERT_TRUE(dict->GetString("deviceId", &device.device_id));
      ASSERT_TRUE(dict->GetString("kind", &device.kind));
      ASSERT_TRUE(dict->GetString("label", &device.label));
      ASSERT_TRUE(dict->GetString("groupId", &device.group_id));

      // Should be HMAC SHA256.
      if (!media::AudioDeviceDescription::IsDefaultDevice(device.device_id) &&
          !(device.device_id ==
            media::AudioDeviceDescription::kCommunicationsDeviceId)) {
        EXPECT_EQ(64ul, device.device_id.length());
        EXPECT_TRUE(
            base::ContainsOnlyChars(device.device_id, "0123456789abcdef"));
      }

      EXPECT_TRUE(device.kind == kDeviceKindAudioInput ||
                  device.kind == kDeviceKindVideoInput ||
                  device.kind == kDeviceKindAudioOutput);
      if (device.kind == kDeviceKindAudioInput) {
        found_audio_input = true;
      } else if (device.kind == kDeviceKindVideoInput) {
        found_video_input = true;
      }

      // enumerateDevices doesn't have group ID support for video input devices.
      // TODO(guidou): remove this once http://crbug.com/627793 is fixed.
      if (device.kind == kDeviceKindVideoInput) {
        EXPECT_TRUE(device.group_id.empty());
      } else {
        EXPECT_FALSE(device.group_id.empty());
      }

      devices->push_back(device);
    }

    EXPECT_TRUE(found_audio_input);
    EXPECT_TRUE(found_video_input);
  }

  bool has_audio_output_devices_initialized_;
  bool has_audio_output_devices_;
};

static const bool kParamsToRunTestsWith[] = { false, true };
INSTANTIATE_TEST_CASE_P(WebRtcGetMediaDevicesBrowserTests,
                        WebRtcGetMediaDevicesBrowserTest,
                        testing::ValuesIn(kParamsToRunTestsWith));

IN_PROC_BROWSER_TEST_P(WebRtcGetMediaDevicesBrowserTest,
                       EnumerateDevicesWithoutAccess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  // Labels should be empty if access has not been allowed.
  for (std::vector<MediaDeviceInfo>::iterator it = devices.begin();
       it != devices.end(); ++it) {
    EXPECT_TRUE(it->label.empty());
  }
}

IN_PROC_BROWSER_TEST_P(WebRtcGetMediaDevicesBrowserTest,
                       EnumerateDevicesWithAccess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_TRUE(GetUserMediaAndAccept(tab));

  std::vector<MediaDeviceInfo> devices;
  EnumerateDevices(tab, &devices);

  // Labels should be non-empty if access has been allowed.
  for (std::vector<MediaDeviceInfo>::iterator it = devices.begin();
       it != devices.end(); ++it) {
    EXPECT_TRUE(!it->label.empty());
  }
}
