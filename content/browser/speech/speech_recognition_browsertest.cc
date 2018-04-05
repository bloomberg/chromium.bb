// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <list>
#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/speech/proto/google_streaming_api.pb.h"
#include "content/browser/speech/speech_recognition_engine.h"
#include "content/browser/speech/speech_recognition_manager_impl.h"
#include "content/browser/speech/speech_recognizer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/audio/mock_audio_manager.h"
#include "media/audio/test_audio_input_controller_factory.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::RunLoop;

namespace content {

namespace {

std::string MakeGoodResponse() {
  proto::SpeechRecognitionEvent proto_event;
  proto_event.set_status(proto::SpeechRecognitionEvent::STATUS_SUCCESS);
  proto::SpeechRecognitionResult* proto_result = proto_event.add_result();
  SpeechRecognitionResult result;
  result.hypotheses.push_back(SpeechRecognitionHypothesis(
      base::UTF8ToUTF16("Pictures of the moon"), 1.0F));
  proto_result->set_final(!result.is_provisional);
  for (size_t i = 0; i < result.hypotheses.size(); ++i) {
    proto::SpeechRecognitionAlternative* proto_alternative =
        proto_result->add_alternative();
    const SpeechRecognitionHypothesis& hypothesis = result.hypotheses[i];
    proto_alternative->set_confidence(hypothesis.confidence);
    proto_alternative->set_transcript(base::UTF16ToUTF8(hypothesis.utterance));
  }

  std::string msg_string;
  proto_event.SerializeToString(&msg_string);

  // Prepend 4 byte prefix length indication to the protobuf message as
  // envisaged by the google streaming recognition webservice protocol.
  uint32_t prefix =
      base::HostToNet32(base::checked_cast<uint32_t>(msg_string.size()));
  msg_string.insert(0, reinterpret_cast<char*>(&prefix), sizeof(prefix));
  return msg_string;
}

}  // namespace

class SpeechRecognitionBrowserTest
    : public ContentBrowserTest,
      public media::TestAudioInputControllerDelegate {
 public:
  enum StreamingServerState {
    kIdle,
    kTestAudioControllerOpened,
    kTestAudioControllerClosed,
  };

  // media::TestAudioInputControllerDelegate methods.
  void TestAudioControllerOpened(
      media::TestAudioInputController* controller) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ASSERT_EQ(kIdle, streaming_server_state_);
    streaming_server_state_ = kTestAudioControllerOpened;
    const int capture_packet_interval_ms =
        (1000 * controller->audio_parameters().frames_per_buffer()) /
        controller->audio_parameters().sample_rate();
    ASSERT_EQ(SpeechRecognitionEngine::kAudioPacketIntervalMs,
        capture_packet_interval_ms);
    FeedAudioController(500 /* ms */, /*noise=*/ false);
    FeedAudioController(1000 /* ms */, /*noise=*/ true);
    FeedAudioController(1000 /* ms */, /*noise=*/ false);
  }

  void TestAudioControllerClosed(
      media::TestAudioInputController* controller) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    ASSERT_EQ(kTestAudioControllerOpened, streaming_server_state_);
    streaming_server_state_ = kTestAudioControllerClosed;

    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::BindOnce(&SpeechRecognitionBrowserTest::SendResponse,
                       base::Unretained(this)));
  }

  void SendResponse() {}

  // Helper methods used by test fixtures.
  GURL GetTestUrlFromFragment(const std::string& fragment) {
    return GURL(GetTestUrl("speech", "web_speech_recognition.html").spec() +
        "#" + fragment);
  }

  std::string GetPageFragment() {
    return shell()->web_contents()->GetLastCommittedURL().ref();
  }

  const StreamingServerState &streaming_server_state() {
    return streaming_server_state_;
  }

 protected:
  // ContentBrowserTest methods.
  void SetUpOnMainThread() override {
    test_audio_input_controller_factory_.set_delegate(this);
    media::AudioInputController::set_factory_for_testing(
        &test_audio_input_controller_factory_);
    streaming_server_state_ = kIdle;

    ASSERT_TRUE(SpeechRecognitionManagerImpl::GetInstance());
    media::AudioManager::StartHangMonitorIfNeeded(
        BrowserThread::GetTaskRunnerForThread(BrowserThread::IO));
    audio_manager_ = std::make_unique<media::MockAudioManager>(
        std::make_unique<media::AudioThreadImpl>());
    audio_manager_->SetInputStreamParameters(
        media::AudioParameters::UnavailableDeviceParams());
    audio_system_ =
        std::make_unique<media::AudioSystemImpl>(audio_manager_.get());
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(audio_system_.get(),
                                                        audio_manager_.get());
  }

  void TearDownOnMainThread() override {
    SpeechRecognizerImpl::SetAudioEnvironmentForTesting(nullptr, nullptr);

    audio_manager_->Shutdown();

    test_audio_input_controller_factory_.set_delegate(nullptr);
  }

 private:
  static void FeedSingleBufferToAudioController(
      scoped_refptr<media::TestAudioInputController> controller,
      size_t buffer_size,
      bool fill_with_noise) {
    DCHECK(controller.get());
    const media::AudioParameters& audio_params = controller->audio_parameters();
    std::unique_ptr<uint8_t[]> audio_buffer(new uint8_t[buffer_size]);
    if (fill_with_noise) {
      for (size_t i = 0; i < buffer_size; ++i)
        audio_buffer[i] =
            static_cast<uint8_t>(127 * sin(i * 3.14F / (16 * buffer_size)));
    } else {
      memset(audio_buffer.get(), 0, buffer_size);
    }

    std::unique_ptr<media::AudioBus> audio_bus =
        media::AudioBus::Create(audio_params);
    audio_bus->FromInterleaved(&audio_buffer.get()[0],
                               audio_bus->frames(),
                               audio_params.bits_per_sample() / 8);
    controller->sync_writer()->Write(audio_bus.get(), 0.0, false,
                                     base::TimeTicks::Now());
  }

  void FeedAudioController(int duration_ms, bool feed_with_noise) {
    media::TestAudioInputController* controller =
        test_audio_input_controller_factory_.controller();
    ASSERT_TRUE(controller);
    const media::AudioParameters& audio_params = controller->audio_parameters();
    const size_t buffer_size = audio_params.GetBytesPerBuffer();
    const int ms_per_buffer = audio_params.frames_per_buffer() * 1000 /
                              audio_params.sample_rate();
    // We can only simulate durations that are integer multiples of the
    // buffer size. In this regard see
    // SpeechRecognitionEngine::GetDesiredAudioChunkDurationMs().
    ASSERT_EQ(0, duration_ms % ms_per_buffer);

    const int n_buffers = duration_ms / ms_per_buffer;
    for (int i = 0; i < n_buffers; ++i) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &FeedSingleBufferToAudioController,
              scoped_refptr<media::TestAudioInputController>(controller),
              buffer_size, feed_with_noise));
    }
  }

  std::unique_ptr<media::MockAudioManager> audio_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  StreamingServerState streaming_server_state_;

  media::TestAudioInputControllerFactory test_audio_input_controller_factory_;
};

// Simply loads the test page and checks if it was able to create a Speech
// Recognition object in JavaScript, to make sure the Web Speech API is enabled.
// Flaky on all platforms. http://crbug.com/396414.
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, DISABLED_Precheck) {
  NavigateToURLBlockUntilNavigationsComplete(
      shell(), GetTestUrlFromFragment("precheck"), 2);

  EXPECT_EQ(kIdle, streaming_server_state());
  EXPECT_EQ("success", GetPageFragment());
}

// Flaky on mac, see https://crbug.com/794645.
#if defined(OS_MACOSX)
#define MAYBE_OneShotRecognition DISABLED_OneShotRecognition
#else
#define MAYBE_OneShotRecognition OneShotRecognition
#endif
IN_PROC_BROWSER_TEST_F(SpeechRecognitionBrowserTest, MAYBE_OneShotRecognition) {
  // Set up a test server, with two response handlers.
  net::test_server::ControllableHttpResponse upstream_response(
      embedded_test_server(), "/foo/up?", true /* relative_url_is_prefix */);
  net::test_server::ControllableHttpResponse downstream_response(
      embedded_test_server(), "/foo/down?", true /* relative_url_is_prefix */);
  ASSERT_TRUE(embedded_test_server()->Start());
  // Use a base path that doesn't end in a slash to mimic the default URL.
  std::string web_service_base_url =
      embedded_test_server()->base_url().spec() + "foo";
  SpeechRecognitionEngine::set_web_service_base_url_for_tests(
      web_service_base_url.c_str());

  // Need to watch for two navigations. Can't use
  // NavigateToURLBlockUntilNavigationsComplete so that the
  // ControllableHttpResponses can be used to wait for the test server to see
  // the network requests, and response to them.
  TestNavigationObserver navigation_observer(shell()->web_contents(), 2);
  shell()->LoadURL(GetTestUrlFromFragment("oneshot"));

  // Wait for the upstream HTTP request to be completely received, and return an
  // empty response.
  upstream_response.WaitForRequest();
  EXPECT_FALSE(upstream_response.http_request()->content.empty());
  EXPECT_EQ(net::test_server::METHOD_POST,
            upstream_response.http_request()->method);
  EXPECT_EQ("chunked",
            upstream_response.http_request()->headers.at("Transfer-Encoding"));
  EXPECT_EQ("audio/x-flac; rate=16000",
            upstream_response.http_request()->headers.at("Content-Type"));
  upstream_response.Send("HTTP/1.1 200 OK\r\n\r\n");
  upstream_response.Done();

  // Wait for the downstream HTTP request to be received, and response with a
  // valid response.
  downstream_response.WaitForRequest();
  EXPECT_EQ(net::test_server::METHOD_GET,
            downstream_response.http_request()->method);
  downstream_response.Send("HTTP/1.1 200 OK\r\n\r\n" + MakeGoodResponse());
  downstream_response.Done();

  navigation_observer.Wait();

  EXPECT_EQ(kTestAudioControllerClosed, streaming_server_state());
  EXPECT_EQ("goodresult1", GetPageFragment());

  // Remove reference to URL string that's on the stack.
  SpeechRecognitionEngine::set_web_service_base_url_for_tests(nullptr);
}

}  // namespace content
