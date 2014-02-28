// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/webrtc_content_browsertest_base.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/perf/perf_test.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

using trace_analyzer::TraceAnalyzer;
using trace_analyzer::Query;
using trace_analyzer::TraceEventVector;

namespace {

static const char kGetUserMediaAndStop[] = "getUserMediaAndStop";
static const char kGetUserMediaAndWaitAndStop[] = "getUserMediaAndWaitAndStop";
static const char kGetUserMediaAndAnalyseAndStop[] =
    "getUserMediaAndAnalyseAndStop";

// Results returned by JS.
static const char kOK[] = "OK";
static const char kGetUserMediaFailed[] =
    "GetUserMedia call failed with code undefined";

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

}  // namespace

namespace content {

class WebRtcGetUserMediaBrowserTest: public WebRtcContentBrowserTest {
 public:
  WebRtcGetUserMediaBrowserTest() : trace_log_(NULL) {}
  virtual ~WebRtcGetUserMediaBrowserTest() {}

  void StartTracing() {
    CHECK(trace_log_ == NULL) << "Can only can start tracing once";
    trace_log_ = base::debug::TraceLog::GetInstance();
    trace_log_->SetEnabled(base::debug::CategoryFilter("video"),
                           base::debug::TraceLog::RECORDING_MODE,
                           base::debug::TraceLog::ENABLE_SAMPLING);
    // Check that we are indeed recording.
    EXPECT_EQ(trace_log_->GetNumTracesRecorded(), 1);
  }

  void StopTracing() {
    CHECK(message_loop_runner_ == NULL) << "Calling StopTracing more than once";
    trace_log_->SetDisabled();
    message_loop_runner_ = new MessageLoopRunner;
    trace_log_->Flush(base::Bind(
        &WebRtcGetUserMediaBrowserTest::OnTraceDataCollected,
        base::Unretained(this)));
    message_loop_runner_->Run();
  }

  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events) {
    CHECK(!has_more_events);
    recorded_trace_data_ = events_str_ptr;
    message_loop_runner_->Quit();
  }

  TraceAnalyzer* CreateTraceAnalyzer() {
    return TraceAnalyzer::Create("[" + recorded_trace_data_->data() + "]");
  }

  void RunGetUserMediaAndCollectMeasures(const int time_to_sample_secs,
                                         const std::string& measure_filter,
                                         const std::string& graph_name) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);
    // Put getUserMedia to work and let it run for a couple of seconds.
    DCHECK(time_to_sample_secs);
    ASSERT_TRUE(
        ExecuteJavascript(base::StringPrintf("%s({video: true}, %d);",
                                             kGetUserMediaAndWaitAndStop,
                                             time_to_sample_secs)));

    // Make sure the stream is up and running, then start collecting traces.
    ExpectTitle("Running...");
    StartTracing();

    // Wait until the page title changes to "OK". Do not sleep() here since that
    // would stop both this code and the browser underneath.
    ExpectTitle("OK");
    StopTracing();

    scoped_ptr<TraceAnalyzer> analyzer(CreateTraceAnalyzer());
    analyzer->AssociateBeginEndEvents();
    trace_analyzer::TraceEventVector events;
    DCHECK(measure_filter.size());
    analyzer->FindEvents(
        Query::EventNameIs(measure_filter),
        &events);
    ASSERT_GT(events.size(), 0u)
        << "Could not collect any samples during test, this is bad";

    std::string duration_us;
    std::string interarrival_us;
    for (size_t i = 0; i != events.size(); ++i) {
      duration_us.append(
          base::StringPrintf("%d,", static_cast<int>(events[i]->duration)));
    }

    for (size_t i = 1; i < events.size(); ++i) {
      // The event |timestamp| comes in ns, divide to get us like |duration|.
      interarrival_us.append(base::StringPrintf("%d,",
          static_cast<int>((events[i]->timestamp - events[i - 1]->timestamp) /
                           base::Time::kNanosecondsPerMicrosecond)));
    }

    perf_test::PrintResultList(
        graph_name, "", "sample_duration", duration_us, "us", true);

    perf_test::PrintResultList(
        graph_name, "", "interarrival_time", interarrival_us, "us", true);
  }

  void GetSources(std::vector<std::string>* audio_ids,
                  std::vector<std::string>* video_ids) {
    GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
    NavigateToURL(shell(), url);

    std::string sources_as_json = ExecuteJavascriptAndReturnResult(
        "getSources()");
    EXPECT_FALSE(sources_as_json.empty());

    int error_code;
    std::string error_message;
    scoped_ptr<base::Value> value(
        base::JSONReader::ReadAndReturnError(sources_as_json,
                                             base::JSON_ALLOW_TRAILING_COMMAS,
                                             &error_code,
                                             &error_message));

    ASSERT_TRUE(value.get() != NULL) << error_message;
    EXPECT_EQ(value->GetType(), base::Value::TYPE_LIST);

    base::ListValue* values;
    ASSERT_TRUE(value->GetAsList(&values));

    for (base::ListValue::iterator it = values->begin();
         it != values->end(); ++it) {
      const base::DictionaryValue* dict;
      std::string kind;
      std::string id;
      ASSERT_TRUE((*it)->GetAsDictionary(&dict));
      ASSERT_TRUE(dict->GetString("kind", &kind));
      ASSERT_TRUE(dict->GetString("id", &id));
      ASSERT_FALSE(id.empty());
      EXPECT_TRUE(kind == "audio" || kind == "video");
      if (kind == "audio") {
        audio_ids->push_back(id);
      } else if (kind == "video") {
        video_ids->push_back(id);
      }
    }
    ASSERT_FALSE(audio_ids->empty());
    ASSERT_FALSE(video_ids->empty());
  }

 private:
  base::debug::TraceLog* trace_log_;
  scoped_refptr<base::RefCountedString> recorded_trace_data_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest, GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript(
      base::StringPrintf("%s({video: true});", kGetUserMediaAndStop)));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript(base::StringPrintf(
      "%s({video: true, audio: true});", kGetUserMediaAndStop)));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript("getUserMediaAndClone();"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test all combinations of mandatory sourceID;
  for (std::vector<std::string>::const_iterator video_it = video_ids.begin();
       video_it != video_ids.end(); ++video_it) {
    for (std::vector<std::string>::const_iterator audio_it = audio_ids.begin();
         audio_it != audio_ids.end(); ++audio_it) {
      NavigateToURL(shell(), url);
      EXPECT_EQ(kOK, ExecuteJavascriptAndReturnResult(
          GenerateGetUserMediaWithMandatorySourceID(
              kGetUserMediaAndStop,
              *audio_it,
              *video_it)));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid mandatory audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ(kGetUserMediaFailed, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithMandatorySourceID(
          kGetUserMediaAndStop,
          "something invalid",
          video_ids[0])));

  // Test with invalid mandatory video sourceID.
  EXPECT_EQ(kGetUserMediaFailed, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithMandatorySourceID(
          kGetUserMediaAndStop,
          audio_ids[0],
          "something invalid")));

  // Test with empty mandatory audio sourceID.
  EXPECT_EQ(kGetUserMediaFailed, ExecuteJavascriptAndReturnResult(
      GenerateGetUserMediaWithMandatorySourceID(
          kGetUserMediaAndStop,
          "",
          video_ids[0])));
}

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       GetUserMediaWithInvalidOptionalSourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

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

IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest, TwoGetUserMediaAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript(
      "twoGetUserMediaAndStop({video: true, audio: true});"));

  ExpectTitle("OK");
}

// This test will make a simple getUserMedia page, verify that video is playing
// in a simple local <video>, and for a couple of seconds, collect some
// performance traces from VideoCaptureController colorspace conversion and
// potential resizing.
IN_PROC_BROWSER_TEST_F(
    WebRtcGetUserMediaBrowserTest,
    TraceVideoCaptureControllerPerformanceDuringGetUserMedia) {
  RunGetUserMediaAndCollectMeasures(
      10,
      "VideoCaptureController::OnIncomingCapturedData",
      "VideoCaptureController");
}

// This test will make a simple getUserMedia page, verify that video is playing
// in a simple local <video>, and for a couple of seconds, collect some
// performance traces. Only for Android.
#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(
    WebRtcGetUserMediaBrowserTest,
    TraceVideoCaptureDeviceAndroidPerformanceDuringGetUserMedia) {
  RunGetUserMediaAndCollectMeasures(
      10,
      "VideoCaptureDeviceAndroid::OnFrameAvailable",
      "VideoCaptureDeviceAndroid");
}
#endif

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_F(WebRtcGetUserMediaBrowserTest,
                       TestGetUserMediaAspectRatio) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string constraints_4_3 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 480, 480, 30, 30);
  std::string constraints_16_9 = GenerateGetUserMediaCall(
      kGetUserMediaAndAnalyseAndStop, 640, 640, 360, 360, 30, 30);

  // TODO(mcasas): add more aspect ratios, in particular 16:10 crbug.com/275594.

  NavigateToURL(shell(), url);
  ASSERT_TRUE(ExecuteJavascript(constraints_4_3));
  ExpectTitle("4:3 letterbox");

  NavigateToURL(shell(), url);
  ASSERT_TRUE(ExecuteJavascript(constraints_16_9));
  ExpectTitle("16:9 letterbox");
}

namespace {

struct UserMediaSizes {
  int min_width;
  int max_width;
  int min_height;
  int max_height;
  int min_frame_rate;
  int max_frame_rate;
};

}  // namespace

class WebRtcConstraintsBrowserTest
    : public WebRtcGetUserMediaBrowserTest,
      public testing::WithParamInterface<UserMediaSizes> {
 public:
  WebRtcConstraintsBrowserTest() : user_media_(GetParam()) {}
  const UserMediaSizes& user_media() const { return user_media_; }

 private:
  UserMediaSizes user_media_;
};

// This test calls getUserMedia in sequence with different constraints.
IN_PROC_BROWSER_TEST_P(WebRtcConstraintsBrowserTest, GetUserMediaConstraints) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  std::string call = GenerateGetUserMediaCall(kGetUserMediaAndStop,
                                              user_media().min_width,
                                              user_media().max_width,
                                              user_media().min_height,
                                              user_media().max_height,
                                              user_media().min_frame_rate,
                                              user_media().max_frame_rate);
  DVLOG(1) << "Calling getUserMedia: " << call;
  NavigateToURL(shell(), url);
  ASSERT_TRUE(ExecuteJavascript(call));
  ExpectTitle("OK");
}

static const UserMediaSizes kAllUserMediaSizes[] = {
    {320, 320, 180, 180, 30, 30},
    {320, 320, 240, 240, 30, 30},
    {640, 640, 360, 360, 30, 30},
    {640, 640, 480, 480, 30, 30},
    {960, 960, 720, 720, 30, 30},
    {1280, 1280, 720, 720, 30, 30},
    {1920, 1920, 1080, 1080, 30, 30}};

INSTANTIATE_TEST_CASE_P(UserMedia,
                        WebRtcConstraintsBrowserTest,
                        testing::ValuesIn(kAllUserMediaSizes));

}  // namespace content
