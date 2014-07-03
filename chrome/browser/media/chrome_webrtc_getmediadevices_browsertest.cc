// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/string_util.h"
#include "chrome/browser/media/webrtc_browsertest_base.h"
#include "chrome/browser/media/webrtc_browsertest_common.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "media/audio/audio_manager.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace {

const char kMainWebrtcTestHtmlPage[] = "/webrtc/webrtc_jsep01_test.html";

const char kDeviceKindAudioInput[] = "audioinput";
const char kDeviceKindVideoInput[] = "videoinput";
const char kDeviceKindAudioOutput[] = "audiooutput";

const char kSourceKindAudioInput[] = "audio";
const char kSourceKindVideoInput[] = "video";

}  // namespace

// Integration test for WebRTC getMediaDevices. It always uses fake devices.
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

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    DetectErrorsInJavaScript();  // Look for errors in our rather complex js.
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
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

  bool HasOutputDevices() {
    // There's no fake audio output devices supported yet. We can't test audio
    // output devices on bots with no output devices, so skip testing for that
    // on such bots. We cache the result since querying for devices can take
    // considerable time.
    if (!has_audio_output_devices_initialized_) {
      has_audio_output_devices_ =
          media::AudioManager::Get()->HasAudioOutputDevices();
      has_audio_output_devices_initialized_ = true;
    }
    return has_audio_output_devices_;
  }

  // If |get_sources| is true, use getSources API and leave groupId empty,
  // otherwise use getMediaDevices API.
  void GetMediaDevicesOrSources(content::WebContents* tab,
                                std::vector<MediaDeviceInfo>* devices,
                                bool get_sources) {
    std::string devices_as_json =
        ExecuteJavascript(get_sources ? "getSources()" : "getMediaDevices()",
                          tab);
    EXPECT_FALSE(devices_as_json.empty());

    int error_code;
    std::string error_message;
    scoped_ptr<base::Value> value(
        base::JSONReader::ReadAndReturnError(devices_as_json,
                                             base::JSON_ALLOW_TRAILING_COMMAS,
                                             &error_code,
                                             &error_message));

    ASSERT_TRUE(value.get() != NULL) << error_message;
    EXPECT_EQ(value->GetType(), base::Value::TYPE_LIST);

    base::ListValue* values;
    ASSERT_TRUE(value->GetAsList(&values));
    ASSERT_FALSE(values->empty());
    bool found_audio_input = false;
    bool found_video_input = false;
    bool found_audio_output = false;

    for (base::ListValue::iterator it = values->begin();
         it != values->end(); ++it) {
      const base::DictionaryValue* dict;
      MediaDeviceInfo device;
      ASSERT_TRUE((*it)->GetAsDictionary(&dict));
      ASSERT_TRUE(dict->GetString(get_sources ? "id" : "deviceId",
                                  &device.device_id));
      ASSERT_TRUE(dict->GetString("kind", &device.kind));
      ASSERT_TRUE(dict->GetString("label", &device.label));
      if (!get_sources)
        ASSERT_TRUE(dict->GetString("groupId", &device.group_id));

      // Should be HMAC SHA256.
      EXPECT_EQ(64ul, device.device_id.length());
      EXPECT_TRUE(base::ContainsOnlyChars(device.device_id,
                                          "0123456789abcdef"));

      const char* kAudioInputKind =
          get_sources ? kSourceKindAudioInput : kDeviceKindAudioInput;
      const char* kVideoInputKind =
          get_sources ? kSourceKindVideoInput : kDeviceKindVideoInput;
      if (get_sources) {
        EXPECT_TRUE(device.kind == kAudioInputKind ||
                    device.kind == kVideoInputKind);
      } else {
        EXPECT_TRUE(device.kind == kAudioInputKind ||
                    device.kind == kVideoInputKind ||
                    device.kind == kDeviceKindAudioOutput);
      }
      if (device.kind == kAudioInputKind) {
        found_audio_input = true;
      } else if (device.kind == kVideoInputKind) {
        found_video_input = true;
      } else {
        found_audio_output = true;
      }

      // getSources doesn't have group ID support. getMediaDevices doesn't have
      // group ID support for video input devices.
      if (get_sources || device.kind == kDeviceKindVideoInput) {
        EXPECT_TRUE(device.group_id.empty());
      } else {
        EXPECT_FALSE(device.group_id.empty());
      }

      devices->push_back(device);
    }

    EXPECT_TRUE(found_audio_input);
    EXPECT_TRUE(found_video_input);
    if (get_sources) {
      EXPECT_FALSE(found_audio_output);
    } else {
      EXPECT_EQ(HasOutputDevices(), found_audio_output);
    }
  }

  void GetMediaDevices(content::WebContents* tab,
                     std::vector<MediaDeviceInfo>* devices) {
    GetMediaDevicesOrSources(tab, devices, false);
  }

  void GetSources(content::WebContents* tab,
                  std::vector<MediaDeviceInfo>* sources) {
    GetMediaDevicesOrSources(tab, sources, true);
  }

  bool has_audio_output_devices_initialized_;
  bool has_audio_output_devices_;
};

static const bool kParamsToRunTestsWith[] = { false, true };
INSTANTIATE_TEST_CASE_P(WebRtcGetMediaDevicesBrowserTests,
                        WebRtcGetMediaDevicesBrowserTest,
                        testing::ValuesIn(kParamsToRunTestsWith));

// getMediaDevices has been removed and will be replaced
// MediaDevices.enumerateDevices. http://crbug.com/388648.
IN_PROC_BROWSER_TEST_P(WebRtcGetMediaDevicesBrowserTest,
                       DISABLED_GetMediaDevicesWithoutAccess) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::vector<MediaDeviceInfo> devices;
  GetMediaDevices(tab, &devices);

  // Labels should be empty if access has not been allowed.
  for (std::vector<MediaDeviceInfo>::iterator it = devices.begin();
       it != devices.end(); ++it) {
    EXPECT_TRUE(it->label.empty());
  }
}

// getMediaDevices has been removed and will be replaced
// MediaDevices.enumerateDevices. http://crbug.com/388648.
// Disabled, fails due to http://crbug.com/382391.
IN_PROC_BROWSER_TEST_P(WebRtcGetMediaDevicesBrowserTest,
                       DISABLED_GetMediaDevicesWithAccess) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GetUserMediaAndAccept(tab);

  std::vector<MediaDeviceInfo> devices;
  GetMediaDevices(tab, &devices);

  // Labels should be non-empty if access has been allowed.
  for (std::vector<MediaDeviceInfo>::iterator it = devices.begin();
       it != devices.end(); ++it) {
    EXPECT_TRUE(!it->label.empty());
  }
}

// getMediaDevices has been removed and will be replaced
// MediaDevices.enumerateDevices. http://crbug.com/388648.
IN_PROC_BROWSER_TEST_P(WebRtcGetMediaDevicesBrowserTest,
                       DISABLED_GetMediaDevicesEqualsGetSourcesWithoutAccess) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::vector<MediaDeviceInfo> devices;
  GetMediaDevices(tab, &devices);

  std::vector<MediaDeviceInfo> sources;
  GetSources(tab, &sources);

  std::vector<MediaDeviceInfo>::iterator sources_it = sources.begin();
  for (std::vector<MediaDeviceInfo>::iterator devices_it = devices.begin();
       devices_it != devices.end(); ++devices_it) {
    if (devices_it->kind == kDeviceKindAudioOutput)
      continue;
    EXPECT_STREQ(devices_it->device_id.c_str(), sources_it->device_id.c_str());
    if (devices_it->kind == kDeviceKindAudioInput) {
      EXPECT_STREQ(kSourceKindAudioInput, sources_it->kind.c_str());
    } else {
      EXPECT_STREQ(kSourceKindVideoInput, sources_it->kind.c_str());
    }
    EXPECT_TRUE(devices_it->label.empty());
    EXPECT_TRUE(sources_it->label.empty());
    ++sources_it;
  }
  EXPECT_EQ(sources.end(), sources_it);
}

// getMediaDevices has been removed and will be replaced
// MediaDevices.enumerateDevices. http://crbug.com/388648.
// Disabled, fails due to http://crbug.com/382391.
IN_PROC_BROWSER_TEST_P(WebRtcGetMediaDevicesBrowserTest,
                       DISABLED_GetMediaDevicesEqualsGetSourcesWithAccess) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  GURL url(embedded_test_server()->GetURL(kMainWebrtcTestHtmlPage));
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  GetUserMediaAndAccept(tab);

  std::vector<MediaDeviceInfo> devices;
  GetMediaDevices(tab, &devices);

  std::vector<MediaDeviceInfo> sources;
  GetSources(tab, &sources);

  std::vector<MediaDeviceInfo>::iterator sources_it = sources.begin();
  for (std::vector<MediaDeviceInfo>::iterator devices_it = devices.begin();
       devices_it != devices.end(); ++devices_it) {
    if (devices_it->kind == kDeviceKindAudioOutput)
      continue;
    EXPECT_STREQ(devices_it->device_id.c_str(), sources_it->device_id.c_str());
    if (devices_it->kind == kDeviceKindAudioInput) {
      EXPECT_STREQ(kSourceKindAudioInput, sources_it->kind.c_str());
    } else {
      EXPECT_STREQ(kSourceKindVideoInput, sources_it->kind.c_str());
    }
    EXPECT_TRUE(!devices_it->label.empty());
    EXPECT_STREQ(devices_it->label.c_str(), sources_it->label.c_str());
    ++sources_it;
  }
  EXPECT_EQ(sources.end(), sources_it);
}
