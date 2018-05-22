// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/json/json_reader.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/webrtc/webrtc_content_browsertest_base.h"
#include "content/browser/webrtc/webrtc_internals.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_manager.h"
#include "media/audio/fake_audio_input_stream.h"
#include "media/base/media_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest-param-test.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace {

static const char kGetUserMediaAndStop[] = "getUserMediaAndStop";
static const char kGetUserMediaAndAnalyseAndStop[] =
    "getUserMediaAndAnalyseAndStop";
static const char kGetUserMediaAndExpectFailure[] =
    "getUserMediaAndExpectFailure";
static const char kGetUserMediaForAudioMutingTest[] =
    "getUserMediaForAudioMutingTest";
static const char kRenderSameTrackMediastreamAndStop[] =
    "renderSameTrackMediastreamAndStop";
static const char kRenderClonedMediastreamAndStop[] =
    "renderClonedMediastreamAndStop";
static const char kRenderClonedTrackMediastreamAndStop[] =
    "renderClonedTrackMediastreamAndStop";
static const char kRenderDuplicatedMediastreamAndStop[] =
    "renderDuplicatedMediastreamAndStop";

// Results returned by JS.
static const char kOK[] = "OK";

std::string GenerateGetUserMediaWithMandatorySourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {mandatory: { sourceId:\"" + audio_source_id + "\"}}, ";

  const std::string video_constraint =
      "video: {mandatory: { sourceId:\"" + video_source_id + "\"}}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

std::string GenerateGetUserMediaWithOptionalSourceID(
    const std::string& function_name,
    const std::string& audio_source_id,
    const std::string& video_source_id) {
  const std::string audio_constraint =
      "audio: {optional: [{sourceId:\"" + audio_source_id + "\"}]}, ";

  const std::string video_constraint =
      "video: {optional: [{ sourceId:\"" + video_source_id + "\"}]}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

std::string GenerateGetUserMediaWithDisableLocalEcho(
    const std::string& function_name,
    const std::string& disable_local_echo) {
  const std::string audio_constraint =
      "audio:{mandatory: { chromeMediaSource : 'system', disableLocalEcho : " +
      disable_local_echo + " }},";

  const std::string video_constraint =
      "video: { mandatory: { chromeMediaSource:'screen' }}";
  return function_name + "({" + audio_constraint + video_constraint + "});";
}

bool VerifyDisableLocalEcho(bool expect_value,
                            const content::StreamControls& controls) {
  return expect_value == controls.disable_local_echo;
}

}  // namespace

namespace content {

class WebRtcGetUserMediaBrowserTest : public WebRtcContentBrowserTestBase,
                                      public testing::WithParamInterface<bool> {
 public:
  WebRtcGetUserMediaBrowserTest() {
    // Automatically grant device permission.
    AppendUseFakeUIForMediaStreamFlag();
    std::vector<base::Feature> audio_service_oop_features = {
        features::kAudioServiceAudioStreams,
        features::kAudioServiceOutOfProcess};
    if (GetParam()) {
      // Force audio service out of process to enabled.
      audio_service_features_.InitWithFeatures(audio_service_oop_features, {});
    } else {
      // Force audio service out of process to disabled.
      audio_service_features_.InitWithFeatures({}, audio_service_oop_features);
    }
  }
  ~WebRtcGetUserMediaBrowserTest() override {}

  // Runs the JavaScript twoGetUserMedia with |constraints1| and |constraint2|.
  void RunTwoGetTwoGetUserMediaWithDifferentContraints(
      const std::string& constraints1,
      const std::string& constraints2,
      const std::string& expected_result) {
    ASSERT_TRUE(embedded_test_server()->Start());

    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);

    std::string command = "twoGetUserMedia(" + constraints1 + ',' +
        constraints2 + ')';

    EXPECT_EQ(expected_result, ExecuteJavascriptAndReturnResult(command));
  }

  void GetInputDevices(std::vector<std::string>* audio_ids,
                       std::vector<std::string>* video_ids) {
    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);

    std::string devices_as_json = ExecuteJavascriptAndReturnResult(
        "getSources()");
    EXPECT_FALSE(devices_as_json.empty());

    int error_code;
    std::string error_message;
    std::unique_ptr<base::Value> value = base::JSONReader::ReadAndReturnError(
        devices_as_json, base::JSON_ALLOW_TRAILING_COMMAS, &error_code,
        &error_message);

    ASSERT_TRUE(value.get() != nullptr) << error_message;
    EXPECT_EQ(value->type(), base::Value::Type::LIST);

    base::ListValue* values;
    ASSERT_TRUE(value->GetAsList(&values));

    for (base::ListValue::iterator it = values->begin();
         it != values->end(); ++it) {
      const base::DictionaryValue* dict;
      std::string kind;
      std::string device_id;
      ASSERT_TRUE(it->GetAsDictionary(&dict));
      ASSERT_TRUE(dict->GetString("kind", &kind));
      ASSERT_TRUE(dict->GetString("id", &device_id));
      ASSERT_FALSE(device_id.empty());
      EXPECT_TRUE(kind == "audio" || kind == "video");
      if (kind == "audio") {
        audio_ids->push_back(device_id);
      } else if (kind == "video") {
        video_ids->push_back(device_id);
      }
    }
    ASSERT_FALSE(audio_ids->empty());
    ASSERT_FALSE(video_ids->empty());
  }

 private:
  base::test::ScopedFeatureList audio_service_features_;
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_GetVideoStreamAndStop DISABLED_GetVideoStreamAndStop
#else
#define MAYBE_GetVideoStreamAndStop GetVideoStreamAndStop
#endif
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});", kGetUserMediaAndStop));
}

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_RenderSameTrackMediastreamAndStop \
  DISABLED_RenderSameTrackMediastreamAndStop
#else
#define MAYBE_RenderSameTrackMediastreamAndStop \
  RenderSameTrackMediastreamAndStop
#endif
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       MAYBE_RenderSameTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                         kRenderSameTrackMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       RenderClonedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);


  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                         kRenderClonedMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       kRenderClonedTrackMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                         kRenderClonedTrackMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       kRenderDuplicatedMediastreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s({video: true});",
                          kRenderDuplicatedMediastreamAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(base::StringPrintf(
      "%s({video: true, audio: true});", kGetUserMediaAndStop));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk("getUserMediaAndClone();");
}

// TODO(crbug.com/803516) : Flaky on all platforms.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       DISABLED_RenderVideoTrackInMultipleTagsAndPause) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk("getUserMediaAndRenderInSeveralVideoTags();");
}

// TODO(crbug.com/571389): Flaky on TSAN bots.
#if defined(OS_LINUX)
#define MAYBE_GetUserMediaWithMandatorySourceID \
  DISABLED_GetUserMediaWithMandatorySourceID
#else
#define MAYBE_GetUserMediaWithMandatorySourceID \
  GetUserMediaWithMandatorySourceID
#endif
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       MAYBE_GetUserMediaWithMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  // Test all combinations of mandatory sourceID;
  for (std::vector<std::string>::const_iterator video_it = video_ids.begin();
       video_it != video_ids.end(); ++video_it) {
    for (std::vector<std::string>::const_iterator audio_it = audio_ids.begin();
         audio_it != audio_ids.end(); ++audio_it) {
      EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
          GenerateGetUserMediaWithMandatorySourceID(
              kGetUserMediaAndStop,
              *audio_it,
              *video_it)));
    }
  }
}
#undef MAYBE_GetUserMediaWithMandatorySourceID

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid mandatory audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ("OverconstrainedError",
            ExecuteJavascriptAndReturnResult(
                GenerateGetUserMediaWithMandatorySourceID(
                    kGetUserMediaAndExpectFailure, "something invalid",
                    video_ids[0])));

  // Test with invalid mandatory video sourceID.
  EXPECT_EQ("OverconstrainedError",
            ExecuteJavascriptAndReturnResult(
                GenerateGetUserMediaWithMandatorySourceID(
                    kGetUserMediaAndExpectFailure, audio_ids[0],
                    "something invalid")));

  // Test with empty mandatory audio sourceID.
  EXPECT_EQ("OverconstrainedError",
            ExecuteJavascriptAndReturnResult(
                GenerateGetUserMediaWithMandatorySourceID(
                    kGetUserMediaAndExpectFailure, "", video_ids[0])));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidOptionalSourceID) {
  ASSERT_TRUE(embedded_test_server()->Start());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetInputDevices(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid optional audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop,
          "something invalid",
          video_ids[0])));

  // Test with invalid optional video sourceID.
  EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop,
          audio_ids[0],
          "something invalid")));

  // Test with empty optional audio sourceID.
  EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop,
          "",
          video_ids[0])));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest, TwoGetUserMediaAndStop) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(
      "twoGetUserMediaAndStop({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TwoGetUserMediaWithEqualConstraints) {
  std::string constraints1 = "{video: true, audio: true}";
  const std::string& constraints2 = constraints1;
  std::string expected_result = "w=640:h=480-w=640:h=480";

  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TwoGetUserMediaWithSecondVideoCropped) {
  std::string constraints1 = "{video: true}";
  std::string constraints2 =
      "{video: {width: {exact: 640}, height: {exact: 360}}}";
  std::string expected_result = "w=640:h=480-w=640:h=360";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Test fails under MSan, http://crbug.com/445745
#if defined(MEMORY_SANITIZER)
#define MAYBE_TwoGetUserMediaWithFirstHdSecondVga \
  DISABLED_TwoGetUserMediaWithFirstHdSecondVga
#else
#define MAYBE_TwoGetUserMediaWithFirstHdSecondVga \
  TwoGetUserMediaWithFirstHdSecondVga
#endif
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       MAYBE_TwoGetUserMediaWithFirstHdSecondVga) {
  std::string constraints1 =
      "{video: {width : {exact: 1280}, height: {exact: 720}}}";
  std::string constraints2 =
      "{video: {width : {exact: 640}, height: {exact: 480}}}";
  std::string expected_result = "w=1280:h=720-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

// Timing out on Windows 7 bot: http://crbug.com/443294
// Flaky: http://crbug.com/660656; possible the test is too perf sensitive.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       DISABLED_TwoGetUserMediaWithFirst1080pSecondVga) {
  std::string constraints1 =
      "{video: {mandatory: {maxWidth:1920 , minWidth:1920 , maxHeight: 1080, "
      "minHeight: 1080}}}";
  std::string constraints2 =
      "{video: {mandatory: {maxWidth:640 , maxHeight: 480}}}";
  std::string expected_result = "w=1920:h=1080-w=640:h=480";
  RunTwoGetTwoGetUserMediaWithDifferentContraints(constraints1, constraints2,
                                                  expected_result);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithTooHighVideoConstraintsValues) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call = GenerateGetUserMediaCall(kGetUserMediaAndExpectFailure,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value,
                                              large_value);
  NavigateToURL(shell(), url);

  EXPECT_EQ("OverconstrainedError", ExecuteJavascriptAndReturnResult(call));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaFailToAccessAudioDevice) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  // Make sure we'll fail creating the audio stream.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFailAudioStreamCreation);

  const std::string call = base::StringPrintf(
      "%s({video: false, audio: true});", kGetUserMediaAndExpectFailure);
  EXPECT_EQ("NotReadableError", ExecuteJavascriptAndReturnResult(call));
}

// This test makes two getUserMedia requests, one with impossible constraints
// that should trigger an error, and one with valid constraints. The test
// verifies getUserMedia can succeed after being given impossible constraints.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TwoGetUserMediaAndCheckCallbackAfterFailure) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  int large_value = 99999;
  const std::string gum_with_impossible_constraints =
    GenerateGetUserMediaCall(kGetUserMediaAndExpectFailure,
                             large_value,
                             large_value,
                             large_value,
                             large_value,
                             large_value,
                             large_value);
  const std::string gum_with_vga_constraints =
    GenerateGetUserMediaCall(kGetUserMediaAndAnalyseAndStop,
                             640, 640, 480, 480, 10, 30);

  ASSERT_EQ("OverconstrainedError",
            ExecuteJavascriptAndReturnResult(gum_with_impossible_constraints));

  ASSERT_EQ("w=640:h=480",
            ExecuteJavascriptAndReturnResult(gum_with_vga_constraints));
}

#if defined(OS_ANDROID) && defined(NDEBUG)
#define MAYBE_TraceVideoCaptureControllerPerformanceDuringGetUserMedia \
  DISABLED_TraceVideoCaptureControllerPerformanceDuringGetUserMedia
#else
#define MAYBE_TraceVideoCaptureControllerPerformanceDuringGetUserMedia \
  TraceVideoCaptureControllerPerformanceDuringGetUserMedia
#endif

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio4To3) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_4_3 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 480, 480, 10, 30);

  NavigateToURL(shell(), url);
  ASSERT_EQ("w=640:h=480",
            ExecuteJavascriptAndReturnResult(constraints_4_3));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio16To9) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_16_9 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 360, 360, 10, 30);

  NavigateToURL(shell(), url);
  ASSERT_EQ("w=640:h=360",
            ExecuteJavascriptAndReturnResult(constraints_16_9));
}

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio1To1) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_1_1 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 320, 320, 320, 320, 10, 30);

  NavigateToURL(shell(), url);
  ASSERT_EQ("w=320:h=320",
            ExecuteJavascriptAndReturnResult(constraints_1_1));
}

// This test calls getUserMedia in an iframe and immediately close the iframe
// in the scope of the success callback.
// Flaky: crbug.com/727601.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       DISABLED_AudioInIFrameAndCloseInSuccessCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  std::string call =
      "getUserMediaInIframeAndCloseInSuccessCb({audio: true});";
  ExecuteJavascriptAndWaitForOk(call);
}

// Flaky: crbug.com/807638
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       DISABLED_VideoInIFrameAndCloseInSuccessCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  std::string call =
      "getUserMediaInIframeAndCloseInSuccessCb({video: true});";
  ExecuteJavascriptAndWaitForOk(call);
}

// This test calls getUserMedia in an iframe and immediately close the iframe
// in the scope of the failure callback.
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       VideoWithBadConstraintsInIFrameAndCloseInFailureCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  int large_value = 99999;
  std::string call =
      GenerateGetUserMediaCall("getUserMediaInIframeAndCloseInFailureCb",
                               large_value,
                               large_value,
                               large_value,
                               large_value,
                               large_value,
                               large_value);
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(call);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       InvalidSourceIdInIFrameAndCloseInFailureCb) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string call =
      GenerateGetUserMediaWithMandatorySourceID(
          "getUserMediaInIframeAndCloseInFailureCb", "invalid", "invalid");
  NavigateToURL(shell(), url);

  ExecuteJavascriptAndWaitForOk(call);
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       DisableLocalEchoParameter) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  MediaStreamManager* manager =
      BrowserMainLoop::GetInstance()->media_stream_manager();

  manager->SetGenerateStreamCallbackForTesting(
      base::BindOnce(&VerifyDisableLocalEcho, false));
  std::string call = GenerateGetUserMediaWithDisableLocalEcho(
      "getUserMediaAndExpectSuccess", "false");
  ExecuteJavascriptAndWaitForOk(call);

  manager->SetGenerateStreamCallbackForTesting(
      base::BindOnce(&VerifyDisableLocalEcho, true));
  call = GenerateGetUserMediaWithDisableLocalEcho(
      "getUserMediaAndExpectSuccess", "true");
  ExecuteJavascriptAndWaitForOk(call);

  manager->SetGenerateStreamCallbackForTesting(
      MediaStreamManager::GenerateStreamTestCallback());
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest, GetAudioSettingsDefault) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("getAudioSettingsDefault()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioSettingsNoEchoCancellation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("getAudioSettingsNoEchoCancellation()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioSettingsDeviceId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("getAudioSettingsDeviceId()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest, SrcObjectAddVideoTrack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("srcObjectAddVideoTrack()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       SrcObjectReplaceInactiveTracks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("srcObjectReplaceInactiveTracks()");
}

// Flaky on all platforms. https://crbug.com/835332
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       DISABLED_SrcObjectRemoveVideoTrack) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("srcObjectRemoveVideoTrack()");
}

// Flaky on memory and leak sanitizers. https://crbug.com/843844
#if defined(MEMORY_SANITIZER) || defined(LEAK_SANITIZER)
#define MAYBE_SrcObjectRemoveFirstOfTwoVideoTracks \
  DISABLED_SrcObjectRemoveFirstOfTwoVideoTracks
#else
#define MAYBE_SrcObjectRemoveFirstOfTwoVideoTracks \
  SrcObjectRemoveFirstOfTwoVideoTracks
#endif
IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       SrcObjectRemoveFirstOfTwoVideoTracks) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("srcObjectRemoveFirstOfTwoVideoTracks()");
}

// TODO(guidou): Add SrcObjectAddAudioTrack and SrcObjectRemoveAudioTrack tests
// when a straightforward mechanism to detect the presence/absence of audio in a
// media element with an assigned MediaStream becomes available.

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       SrcObjectReassignSameObject) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("srcObjectReassignSameObject()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest, ApplyConstraintsVideo) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("applyConstraintsVideo()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       ApplyConstraintsVideoTwoStreams) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("applyConstraintsVideoTwoStreams()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       ApplyConstraintsVideoOverconstrained) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("applyConstraintsVideoOverconstrained()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       ApplyConstraintsNonDevice) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("applyConstraintsNonDevice()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       ConcurrentGetUserMediaStop) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("concurrentGetUserMediaStop()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaAfterStopElementCapture) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("getUserMediaAfterStopCanvasCapture()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaEchoCancellationOnAndOff) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  ExecuteJavascriptAndWaitForOk("getUserMediaEchoCancellationOnAndOff()");
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioStreamAndCheckMutingInitiallyUnmuted) {
  // Muting tests do not work with the out-of-process audio service.
  // https://crbug.com/843490.
  if (GetParam())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  // Expect stream to initially not be muted
  media::FakeAudioInputStream::SetGlobalMutedState(false);
  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s(false);", kGetUserMediaForAudioMutingTest));

  // Mute
  media::FakeAudioInputStream::SetGlobalMutedState(true);
  EXPECT_EQ("onmute: muted=true, readyState=live",
            ExecuteJavascriptAndReturnResult(
                "failTestAfterTimeout('Got no mute event', 1500);"));
  // Unmute
  media::FakeAudioInputStream::SetGlobalMutedState(false);
  EXPECT_EQ("onunmute: muted=false, readyState=live",
            ExecuteJavascriptAndReturnResult(
                "failTestAfterTimeout('Got no unmute event', 1500);"));
}

IN_PROC_BROWSER_TEST_P(WebRtcGetUserMediaBrowserTest,
                       GetAudioStreamAndCheckMutingInitiallyMuted) {
  // Muting tests do not work with the out-of-process audio service.
  // https://crbug.com/843490.
  if (GetParam())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  // Expect stream to initially be muted
  media::FakeAudioInputStream::SetGlobalMutedState(true);
  ExecuteJavascriptAndWaitForOk(
      base::StringPrintf("%s(true);", kGetUserMediaForAudioMutingTest));

  // Unmute
  media::FakeAudioInputStream::SetGlobalMutedState(false);
  EXPECT_EQ("onunmute: muted=false, readyState=live",
            ExecuteJavascriptAndReturnResult(
                "failTestAfterTimeout('Got no unmute event', 1500);"));

  // Mute
  media::FakeAudioInputStream::SetGlobalMutedState(true);
  EXPECT_EQ("onmute: muted=true, readyState=live",
            ExecuteJavascriptAndReturnResult(
                "failTestAfterTimeout('Got no mute event', 1500);"));
}

// We run these tests with the audio service both in and out of the the browser
// process to have waterfall coverage while the feature rolls out. It should be
// removed after launch.
#if (defined(OS_LINUX) && !defined(CHROME_OS)) || defined(OS_MACOSX) || \
    defined(OS_WIN)
// Supported platforms.
INSTANTIATE_TEST_CASE_P(, WebRtcGetUserMediaBrowserTest, ::testing::Bool());
#else
// Platforms where the out of process audio service is not supported
INSTANTIATE_TEST_CASE_P(,
                        WebRtcGetUserMediaBrowserTest,
                        ::testing::Values(false));
#endif

}  // namespace content
