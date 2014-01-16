// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/trace_event_analyzer.h"
#include "base/values.h"
#include "content/browser/media/webrtc_internals.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "media/audio/audio_manager.h"
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

std::string GenerateGetUserMediaCall(const char* function_name,
                                     int min_width,
                                     int max_width,
                                     int min_height,
                                     int max_height,
                                     int min_frame_rate,
                                     int max_frame_rate) {
  return base::StringPrintf(
      "%s({video: {mandatory: {minWidth: %d, maxWidth: %d, "
      "minHeight: %d, maxHeight: %d, minFrameRate: %d, maxFrameRate: %d}, "
      "optional: []}});",
      function_name,
      min_width,
      max_width,
      min_height,
      max_height,
      min_frame_rate,
      max_frame_rate);
}

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

}

namespace content {

class WebrtcBrowserTest: public ContentBrowserTest {
 public:
  WebrtcBrowserTest() : trace_log_(NULL) {}
  virtual ~WebrtcBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    // These tests require pixel output.
    UseRealGLContexts();
    ContentBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // We need fake devices in this test since we want to run on naked VMs. We
    // assume these switches are set by default in content_browsertests.
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeDeviceForMediaStream));
    ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kUseFakeUIForMediaStream));
  }

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
    trace_log_->Flush(base::Bind(&WebrtcBrowserTest::OnTraceDataCollected,
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

 protected:
  bool ExecuteJavascript(const std::string& javascript) {
    return ExecuteScript(shell()->web_contents(), javascript);
  }

  // Executes |javascript|. The script is required to use
  // window.domAutomationController.send to send a string value back to here.
  std::string ExecuteJavascriptAndReturnResult(const std::string& javascript) {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(shell()->web_contents(),
                                              javascript,
                                              &result));
    return result;
  }

  void ExpectTitle(const std::string& expected_title) const {
    base::string16 expected_title16(base::ASCIIToUTF16(expected_title));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title16);
    EXPECT_EQ(expected_title16, title_watcher.WaitAndGetTitle());
  }

  // Convenience function since most peerconnection-call.html tests just load
  // the page, kick off some javascript and wait for the title to change to OK.
  void MakeTypicalPeerConnectionCall(const std::string& javascript) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

    GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
    NavigateToURL(shell(), url);
    ExecuteTestAndWaitForOk(javascript);
  }

  void ExecuteTestAndWaitForOk(const std::string& javascript) {
#if defined (OS_ANDROID)
    // Always force iSAC 16K on Android for now (Opus is broken).
    ASSERT_TRUE(ExecuteJavascript("forceIsac16KInSdp();"));
#endif

    ASSERT_TRUE(ExecuteJavascript(javascript));
    ExpectTitle("OK");
  }

 private:
  base::debug::TraceLog* trace_log_;
  scoped_refptr<base::RefCountedString> recorded_trace_data_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

// These tests will all make a getUserMedia call with different constraints and
// see that the success callback is called. If the error callback is called or
// none of the callbacks are called the tests will simply time out and fail.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript(
      base::StringPrintf("%s({video: true});", kGetUserMediaAndStop)));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript(base::StringPrintf(
      "%s({video: true, audio: true});", kGetUserMediaAndStop)));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetAudioAndVideoStreamAndClone) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript("getUserMediaAndClone();"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, TwoGetUserMediaAndStop) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  ASSERT_TRUE(ExecuteJavascript(
      "twoGetUserMediaAndStop({video: true, audio: true});"));

  ExpectTitle("OK");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetUserMediaWithMandatorySourceID_1) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  for (size_t j = 0; j < video_ids.size() / 2; ++j) {
    for (size_t i = 0; i < audio_ids.size(); ++i) {
      EXPECT_EQ(kOK,
                ExecuteJavascriptAndReturnResult(
                    GenerateGetUserMediaWithMandatorySourceID(
                        kGetUserMediaAndStop, audio_ids[i], video_ids[j])));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetUserMediaWithMandatorySourceID_2) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  for (size_t j = video_ids.size() / 2; j < video_ids.size(); ++j) {
    for (size_t i = 0; i < audio_ids.size(); ++i) {
      EXPECT_EQ(kOK,
                ExecuteJavascriptAndReturnResult(
                    GenerateGetUserMediaWithMandatorySourceID(
                        kGetUserMediaAndStop, audio_ids[i], video_ids[j])));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       GetUserMediaWithInvalidMandatorySourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid mandatory audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ(kGetUserMediaFailed,
            ExecuteJavascriptAndReturnResult(
                GenerateGetUserMediaWithMandatorySourceID(
                    kGetUserMediaAndStop, "something invalid", video_ids[0])));

  // Test with invalid mandatory video sourceID.
  EXPECT_EQ(kGetUserMediaFailed,
            ExecuteJavascriptAndReturnResult(
                GenerateGetUserMediaWithMandatorySourceID(
                    kGetUserMediaAndStop, audio_ids[0], "something invalid")));

  // Test with empty mandatory audio sourceID.
  EXPECT_EQ(kGetUserMediaFailed,
            ExecuteJavascriptAndReturnResult(
                GenerateGetUserMediaWithMandatorySourceID(
                    kGetUserMediaAndStop, "", video_ids[0])));
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetUserMediaWithOptionalSourceID_1) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  // Test all combinations of mandatory sourceID.
  for (size_t j = 0; j < video_ids.size() / 2; ++j) {
    for (size_t i = 0; i < audio_ids.size(); ++i) {
      EXPECT_EQ(kOK,
                ExecuteJavascriptAndReturnResult(
                    GenerateGetUserMediaWithOptionalSourceID(
                        kGetUserMediaAndStop, audio_ids[i], video_ids[j])));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, GetUserMediaWithOptionalSourceID_2) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);

  // Test all combinations of mandatory sourceID.
  for (size_t j = video_ids.size() / 2; j < video_ids.size(); ++j) {
    for (size_t i = 0; i < audio_ids.size(); ++i) {
      EXPECT_EQ(kOK,
                ExecuteJavascriptAndReturnResult(
                    GenerateGetUserMediaWithOptionalSourceID(
                        kGetUserMediaAndStop, audio_ids[i], video_ids[j])));
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       GetUserMediaWithInvalidOptionalSourceID) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  std::vector<std::string> audio_ids;
  std::vector<std::string> video_ids;
  GetSources(&audio_ids, &video_ids);

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));

  // Test with invalid optional audio sourceID.
  NavigateToURL(shell(), url);
  EXPECT_EQ(
      kOK,
      ExecuteJavascriptAndReturnResult(GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop, "something invalid", video_ids[0])));

  // Test with invalid optional video sourceID.
  EXPECT_EQ(
      kOK,
      ExecuteJavascriptAndReturnResult(GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop, audio_ids[0], "something invalid")));

  // Test with empty optional audio sourceID.
  EXPECT_EQ(
      kOK,
      ExecuteJavascriptAndReturnResult(GenerateGetUserMediaWithOptionalSourceID(
          kGetUserMediaAndStop, "", video_ids[0])));
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CanSetupVideoCall DISABLED_CanSetupVideoCall
#else
#define MAYBE_CanSetupVideoCall CanSetupVideoCall
#endif

// These tests will make a complete PeerConnection-based call and verify that
// video is playing for the call.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanSetupVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true});");
}

// This test will make a simple getUserMedia page, verify that video is playing
// in a simple local <video>, and for a couple of seconds, collect some
// performance traces.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, TracePerformanceDuringGetUserMedia) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  GURL url(embedded_test_server()->GetURL("/media/getusermedia.html"));
  NavigateToURL(shell(), url);
  // Put getUserMedia to work and let it run for a couple of seconds.
  ASSERT_TRUE(ExecuteJavascript(base::StringPrintf(
      "%s({video: true}, 10);", kGetUserMediaAndWaitAndStop)));

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
  analyzer->FindEvents(
      Query::EventNameIs("VideoCaptureController::OnIncomingCapturedFrame"),
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
    interarrival_us.append(base::StringPrintf(
        "%d,",
        static_cast<int>(events[i]->timestamp - events[i - 1]->timestamp)));
  }

  perf_test::PrintResultList(
      "video_capture", "", "sample_duration", duration_us, "us", true);

  perf_test::PrintResultList(
      "video_capture", "", "interarrival_time", interarrival_us, "us", true);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240376
#define MAYBE_CanSetupAudioAndVideoCall DISABLED_CanSetupAudioAndVideoCall
#else
#define MAYBE_CanSetupAudioAndVideoCall CanSetupAudioAndVideoCall
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanSetupAudioAndVideoCall) {
  MakeTypicalPeerConnectionCall("call({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CanSetupCallAndSendDtmf) {
  MakeTypicalPeerConnectionCall("callAndSendDtmf(\'123,abc\');");
}

// TODO(phoglund): this test fails because the peer connection state will be
// stable in the second negotiation round rather than have-local-offer.
// http://crbug.com/293125.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       DISABLED_CanMakeEmptyCallThenAddStreamsAndRenegotiate) {
  const char* kJavascript =
      "callEmptyThenAddOneStreamAndRenegotiate({video: true, audio: true});";
  MakeTypicalPeerConnectionCall(kJavascript);
}

// Below 2 test will make a complete PeerConnection-based call between pc1 and
// pc2, and then use the remote stream to setup a call between pc3 and pc4, and
// then verify that video is received on pc3 and pc4.
// Flaky on win xp. http://crbug.com/304775
#if defined(OS_WIN)
#define MAYBE_CanForwardRemoteStream DISABLED_CanForwardRemoteStream
#define MAYBE_CanForwardRemoteStream720p DISABLED_CanForwardRemoteStream720p
#else
#define MAYBE_CanForwardRemoteStream CanForwardRemoteStream
#define MAYBE_CanForwardRemoteStream720p CanForwardRemoteStream720p
#endif
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanForwardRemoteStream) {
  MakeTypicalPeerConnectionCall(
      "callAndForwardRemoteStream({video: true, audio: true});");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanForwardRemoteStream720p) {
  const std::string javascript = GenerateGetUserMediaCall(
      "callAndForwardRemoteStream", 1280, 1280, 720, 720, 30, 30);
  MakeTypicalPeerConnectionCall(javascript);
}

// This test will make a complete PeerConnection-based call but remove the
// MSID and bundle attribute from the initial offer to verify that
// video is playing for the call even if the initiating client don't support
// MSID. http://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02
#if defined(OS_WIN)
// Disabled for win, see http://crbug.com/235089.
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        DISABLED_CanSetupAudioAndVideoCallWithoutMsidAndBundle
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240373
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        DISABLED_CanSetupAudioAndVideoCallWithoutMsidAndBundle
#else
#define MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle\
        CanSetupAudioAndVideoCallWithoutMsidAndBundle
#endif
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MAYBE_CanSetupAudioAndVideoCallWithoutMsidAndBundle) {
  MakeTypicalPeerConnectionCall("callWithoutMsidAndBundle();");
}

// This test will modify the SDP offer to an unsupported codec, which should
// cause SetLocalDescription to fail.
#if defined(USE_OZONE)
// Disabled for Ozone, see http://crbug.com/315392#c15
#define MAYBE_NegotiateUnsupportedVideoCodec\
    DISABLED_NegotiateUnsupportedVideoCodec
#else
#define MAYBE_NegotiateUnsupportedVideoCodec NegotiateUnsupportedVideoCodec
#endif
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MAYBE_NegotiateUnsupportedVideoCodec) {
  MakeTypicalPeerConnectionCall("negotiateUnsupportedVideoCodec();");
}

// This test will modify the SDP offer to use no encryption, which should
// cause SetLocalDescription to fail.
#if defined(USE_OZONE)
// Disabled for Ozone, see http://crbug.com/315392#c15
#define MAYBE_NegotiateNonCryptoCall DISABLED_NegotiateNonCryptoCall
#else
#define MAYBE_NegotiateNonCryptoCall NegotiateNonCryptoCall
#endif
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_NegotiateNonCryptoCall) {
  MakeTypicalPeerConnectionCall("negotiateNonCryptoCall();");
}

// This test can negotiate an SDP offer that includes a b=AS:xx to control
// the bandwidth for audio and video
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, NegotiateOfferWithBLine) {
  MakeTypicalPeerConnectionCall("negotiateOfferWithBLine();");
}

// This test will make a complete PeerConnection-based call using legacy SDP
// settings: GIce, external SDES, and no BUNDLE.
#if defined(OS_WIN)
// Disabled for win7, see http://crbug.com/235089.
#define MAYBE_CanSetupLegacyCall DISABLED_CanSetupLegacyCall
#elif defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux, see http://crbug.com/240373
#define MAYBE_CanSetupLegacyCall DISABLED_CanSetupLegacyCall
#else
#define MAYBE_CanSetupLegacyCall CanSetupLegacyCall
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CanSetupLegacyCall) {
  MakeTypicalPeerConnectionCall("callWithLegacySdp();");
}

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithDataOnly) {
  MakeTypicalPeerConnectionCall("callWithDataOnly();");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallWithSctpDataOnly) {
  MakeTypicalPeerConnectionCall("callWithSctpDataOnly();");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithDataAndMedia DISABLED_CallWithDataAndMedia
#else
#define MAYBE_CallWithDataAndMedia CallWithDataAndMedia
#endif

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and audio and video tracks.
// TODO(mallinath) - Remove this test after rtp based data channel is disabled.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndMedia();");
}


#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithSctpDataAndMedia DISABLED_CallWithSctpDataAndMedia
#else
#define MAYBE_CallWithSctpDataAndMedia CallWithSctpDataAndMedia
#endif

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MAYBE_CallWithSctpDataAndMedia) {
  MakeTypicalPeerConnectionCall("callWithSctpDataAndMedia();");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithDataAndLaterAddMedia DISABLED_CallWithDataAndLaterAddMedia
#else
// Temporarily disable the test on all platforms. http://crbug.com/293252
#define MAYBE_CallWithDataAndLaterAddMedia DISABLED_CallWithDataAndLaterAddMedia
#endif

// This test will make a PeerConnection-based call and test an unreliable text
// dataChannel and later add an audio and video track.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithDataAndLaterAddMedia) {
  MakeTypicalPeerConnectionCall("callWithDataAndLaterAddMedia();");
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithNewVideoMediaStream DISABLED_CallWithNewVideoMediaStream
#else
#define MAYBE_CallWithNewVideoMediaStream CallWithNewVideoMediaStream
#endif

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithNewVideoMediaStream) {
  MakeTypicalPeerConnectionCall("callWithNewVideoMediaStream();");
}

// This test will make a PeerConnection-based call and send a new Video
// MediaStream that has been created based on a MediaStream created with
// getUserMedia. When video is flowing, the VideoTrack is removed and an
// AudioTrack is added instead.
// TODO(phoglund): This test is manual since not all buildbots has an audio
// input.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MANUAL_CallAndModifyStream) {
  MakeTypicalPeerConnectionCall(
      "callWithNewVideoMediaStreamLaterSwitchToAudio();");
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

class WebrtcUserMediaBrowserTest
    : public WebrtcBrowserTest,
      public testing::WithParamInterface<UserMediaSizes> {
 public:
  WebrtcUserMediaBrowserTest() : user_media_(GetParam()) {}
  const UserMediaSizes& user_media() const { return user_media_; }

 private:
  UserMediaSizes user_media_;
};

// This test calls getUserMedia in sequence with different constraints.
IN_PROC_BROWSER_TEST_P(WebrtcUserMediaBrowserTest, GetUserMediaConstraints) {
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
                        WebrtcUserMediaBrowserTest,
                        testing::ValuesIn(kAllUserMediaSizes));

// This test calls getUserMedia and checks for aspect ratio behavior.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, TestGetUserMediaAspectRatio) {
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

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, AddTwoMediaStreamsToOnePC) {
  MakeTypicalPeerConnectionCall("addTwoMediaStreamsToOneConnection();");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       EstablishAudioVideoCallAndMeasureOutputLevel) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    // Bots with no output devices will force the audio code into a different
    // path where it doesn't manage to set either the low or high latency path.
    // This test will compute useless values in that case, so skip running on
    // such bots (see crbug.com/326338).
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream))
          << "Must run with fake devices since the test will explicitly look "
          << "for the fake device signal.";

  MakeTypicalPeerConnectionCall("callAndEnsureAudioIsPlaying();");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       EstablishAudioVideoCallAndVerifyMutingWorks) {
  if (!media::AudioManager::Get()->HasAudioOutputDevices()) {
    // Bots with no output devices will force the audio code into a different
    // path where it doesn't manage to set either the low or high latency path.
    // This test will compute useless values in that case, so skip running on
    // such bots (see crbug.com/326338).
    LOG(INFO) << "Missing output devices: skipping test...";
    return;
  }

  ASSERT_TRUE(CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kUseFakeDeviceForMediaStream))
          << "Must run with fake devices since the test will explicitly look "
          << "for the fake device signal.";

  MakeTypicalPeerConnectionCall("callAndEnsureAudioMutingWorks();");
}

IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, CallAndVerifyVideoMutingWorks) {
  MakeTypicalPeerConnectionCall("callAndEnsureVideoMutingWorks();");
}

#if defined(OS_WIN) || (defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY))
// Timing out on ARM linux bot: http://crbug.com/238490
// Failing on Windows: http://crbug.com/331035
#define MAYBE_CallWithAecDump DISABLED_CallWithAecDump
#else
#define MAYBE_CallWithAecDump CallWithAecDump
#endif

// This tests will make a complete PeerConnection-based call, verify that
// video is playing for the call, and verify that a non-empty AEC dump file
// exists. The AEC dump is enabled through webrtc-internals, in contrast to
// using a command line flag (tested in webrtc_aecdump_browsertest.cc). The HTML
// and Javascript is bypassed since it would trigger a file picker dialog.
// Instead, the dialog callback FileSelected() is invoked directly. In fact,
// there's never a webrtc-internals page opened at all since that's not needed.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest, MAYBE_CallWithAecDump) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  ExecuteTestAndWaitForOk("call({video: true, audio: true});");

  EXPECT_TRUE(base::PathExists(dump_file));
  int64 file_size = 0;
  EXPECT_TRUE(base::GetFileSize(dump_file, &file_size));
  EXPECT_GT(file_size, 0);

  base::DeleteFile(dump_file, false);
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS) && defined(ARCH_CPU_ARM_FAMILY)
// Timing out on ARM linux bot: http://crbug.com/238490
#define MAYBE_CallWithAecDumpEnabledThenDisabled DISABLED_CallWithAecDumpEnabledThenDisabled
#else
#define MAYBE_CallWithAecDumpEnabledThenDisabled CallWithAecDumpEnabledThenDisabled
#endif

// As above, but enable and disable dump before starting a call. The file should
// be created, but should be empty.
IN_PROC_BROWSER_TEST_F(WebrtcBrowserTest,
                       MAYBE_CallWithAecDumpEnabledThenDisabled) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  // We must navigate somewhere first so that the render process is created.
  NavigateToURL(shell(), GURL(""));

  base::FilePath dump_file;
  ASSERT_TRUE(CreateTemporaryFile(&dump_file));

  // This fakes the behavior of another open tab with webrtc-internals, and
  // enabling AEC dump in that tab, then disabling it.
  WebRTCInternals::GetInstance()->FileSelected(dump_file, -1, NULL);
  WebRTCInternals::GetInstance()->DisableAecDump();

  GURL url(embedded_test_server()->GetURL("/media/peerconnection-call.html"));
  NavigateToURL(shell(), url);
  ExecuteTestAndWaitForOk("call({video: true, audio: true});");

  EXPECT_TRUE(base::PathExists(dump_file));
  int64 file_size = 0;
  EXPECT_TRUE(base::GetFileSize(dump_file, &file_size));
  EXPECT_EQ(0, file_size);

  base::DeleteFile(dump_file, false);
}


}  // namespace content
