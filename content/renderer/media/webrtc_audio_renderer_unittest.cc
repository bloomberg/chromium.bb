// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_audio_renderer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "content/public/renderer/media_stream_audio_renderer.h"
#include "content/renderer/media/audio_device_factory.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc_audio_device_impl.h"
#include "media/audio/audio_output_device.h"
#include "media/audio/audio_output_ipc.h"
#include "media/base/audio_bus.h"
#include "media/base/mock_audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStream.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/web/WebHeap.h"
#include "third_party/libjingle/source/talk/app/webrtc/mediastreaminterface.h"

using testing::Return;
using testing::_;

namespace content {

namespace {

const int kHardwareSampleRate = 44100;
const int kHardwareBufferSize = 512;
const char kDefaultOutputDeviceId[] = "";
const char kOtherOutputDeviceId[] = "other-output-device";
const char kInvalidOutputDeviceId[] = "invalid-device";

class MockAudioOutputIPC : public media::AudioOutputIPC {
 public:
  MockAudioOutputIPC() {}
  virtual ~MockAudioOutputIPC() {}

  MOCK_METHOD4(RequestDeviceAuthorization,
               void(media::AudioOutputIPCDelegate* delegate,
                    int session_id,
                    const std::string& device_id,
                    const url::Origin& security_origin));
  MOCK_METHOD2(CreateStream,
               void(media::AudioOutputIPCDelegate* delegate,
                    const media::AudioParameters& params));
  MOCK_METHOD0(PlayStream, void());
  MOCK_METHOD0(PauseStream, void());
  MOCK_METHOD0(CloseStream, void());
  MOCK_METHOD1(SetVolume, void(double volume));
  MOCK_METHOD2(SwitchOutputDevice,
               void(const std::string& device_id,
                    const url::Origin& security_origin));
};

class FakeAudioOutputDevice
    : NON_EXPORTED_BASE(public media::AudioOutputDevice) {
 public:
  FakeAudioOutputDevice(
      scoped_ptr<media::AudioOutputIPC> ipc,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const std::string& device_id)
      : AudioOutputDevice(std::move(ipc),
                          io_task_runner,
                          0,
                          std::string(),
                          url::Origin()),
        device_id_(device_id) {}
  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD0(Play, void());
  MOCK_METHOD1(SetVolume, bool(double volume));

  media::OutputDeviceStatus GetDeviceStatus() override {
    return device_id_ == kInvalidOutputDeviceId
               ? media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL
               : media::OUTPUT_DEVICE_STATUS_OK;
  }

  std::string GetDeviceId() const { return device_id_; }

  media::AudioParameters GetOutputParameters() override {
    return media::AudioParameters(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                                  media::CHANNEL_LAYOUT_STEREO,
                                  kHardwareSampleRate, 16, kHardwareBufferSize);
  }

 protected:
  virtual ~FakeAudioOutputDevice() {}

 private:
  const std::string device_id_;
};

class MockAudioRendererSource : public WebRtcAudioRendererSource {
 public:
  MockAudioRendererSource() {}
  virtual ~MockAudioRendererSource() {}
  MOCK_METHOD4(RenderData, void(media::AudioBus* audio_bus,
                                int sample_rate,
                                int audio_delay_milliseconds,
                                base::TimeDelta* current_time));
  MOCK_METHOD1(RemoveAudioRenderer, void(WebRtcAudioRenderer* renderer));
  MOCK_METHOD0(AudioRendererThreadStopped, void());
};

}  // namespace

class WebRtcAudioRendererTest : public testing::Test,
                                public AudioDeviceFactory {
 public:
  MOCK_METHOD1(MockSwitchDeviceCallback, void(media::OutputDeviceStatus));
  void SwitchDeviceCallback(base::RunLoop* loop,
                            media::OutputDeviceStatus result) {
    MockSwitchDeviceCallback(result);
    loop->Quit();
  }

 protected:
  WebRtcAudioRendererTest()
      : message_loop_(new base::MessageLoopForIO),
        mock_ipc_(nullptr),
        source_(new MockAudioRendererSource()) {
    blink::WebVector<blink::WebMediaStreamTrack> dummy_tracks;
    stream_.initialize("new stream", dummy_tracks, dummy_tracks);
  }

  void SetupRenderer(const std::string& device_id) {
    renderer_ = new WebRtcAudioRenderer(message_loop_->task_runner(), stream_,
                                        1, 1, device_id, url::Origin());
    EXPECT_CALL(*this, MockCreateOutputDevice(1, _, device_id, _));
    EXPECT_TRUE(renderer_->Initialize(source_.get()));

    renderer_proxy_ = renderer_->CreateSharedAudioRendererProxy(stream_);
  }

  MOCK_METHOD1(CreateInputDevice, media::AudioInputDevice*(int));
  MOCK_METHOD4(MockCreateOutputDevice,
               media::AudioOutputDevice*(int,
                                         int,
                                         const std::string&,
                                         const url::Origin&));
  media::AudioOutputDevice* CreateOutputDevice(
      int render_frame_id,
      int session_id,
      const std::string& device_id,
      const url::Origin& security_origin) {
    MockAudioOutputIPC* fake_ipc = new MockAudioOutputIPC();
    FakeAudioOutputDevice* fake_device =
        new FakeAudioOutputDevice(scoped_ptr<media::AudioOutputIPC>(fake_ipc),
                                  message_loop_->task_runner(), device_id);
    if (device_id != kInvalidOutputDeviceId) {
      mock_output_device_ = fake_device;
      mock_ipc_ = fake_ipc;
      EXPECT_CALL(*mock_output_device_.get(), Start());
    }

    MockCreateOutputDevice(render_frame_id, session_id, device_id,
                           security_origin);
    return fake_device;
  }

  void TearDown() override {
    renderer_proxy_ = nullptr;
    renderer_ = nullptr;
    stream_.reset();
    source_.reset();
    mock_output_device_ = nullptr;
    blink::WebHeap::collectAllGarbageForTesting();
  }

  // Used to construct |mock_output_device_|.
  scoped_ptr<base::MessageLoopForIO> message_loop_;
  MockAudioOutputIPC* mock_ipc_;  // Owned by AudioOuputDevice.

  scoped_refptr<FakeAudioOutputDevice> mock_output_device_;
  scoped_ptr<MockAudioRendererSource> source_;
  blink::WebMediaStream stream_;
  scoped_refptr<WebRtcAudioRenderer> renderer_;
  scoped_refptr<MediaStreamAudioRenderer> renderer_proxy_;
};

// Verify that the renderer will be stopped if the only proxy is stopped.
TEST_F(WebRtcAudioRendererTest, StopRenderer) {
  SetupRenderer(kDefaultOutputDeviceId);
  renderer_proxy_->Start();

  // |renderer_| has only one proxy, stopping the proxy should stop the sink of
  // |renderer_|.
  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

// Verify that the renderer will not be stopped unless the last proxy is
// stopped.
TEST_F(WebRtcAudioRendererTest, MultipleRenderers) {
  SetupRenderer(kDefaultOutputDeviceId);
  renderer_proxy_->Start();

  // Create a vector of renderer proxies from the |renderer_|.
  std::vector<scoped_refptr<MediaStreamAudioRenderer> > renderer_proxies_;
  static const int kNumberOfRendererProxy = 5;
  for (int i = 0; i < kNumberOfRendererProxy; ++i) {
    scoped_refptr<MediaStreamAudioRenderer> renderer_proxy(
        renderer_->CreateSharedAudioRendererProxy(stream_));
    renderer_proxy->Start();
    renderer_proxies_.push_back(renderer_proxy);
  }

  // Stop the |renderer_proxy_| should not stop the sink since it is used by
  // other proxies.
  EXPECT_CALL(*mock_output_device_.get(), Stop()).Times(0);
  renderer_proxy_->Stop();

  for (int i = 0; i < kNumberOfRendererProxy; ++i) {
    if (i != kNumberOfRendererProxy -1) {
      EXPECT_CALL(*mock_output_device_.get(), Stop()).Times(0);
    } else {
      // When the last proxy is stopped, the sink will stop.
      EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
      EXPECT_CALL(*mock_output_device_.get(), Stop());
    }
    renderer_proxies_[i]->Stop();
  }
}

// Verify that the sink of the renderer is using the expected sample rate and
// buffer size.
TEST_F(WebRtcAudioRendererTest, VerifySinkParameters) {
  SetupRenderer(kDefaultOutputDeviceId);
  renderer_proxy_->Start();
#if defined(OS_LINUX) || defined(OS_MACOSX)
  static const int kExpectedBufferSize = kHardwareSampleRate / 100;
#elif defined(OS_ANDROID)
  static const int kExpectedBufferSize = 2 * kHardwareSampleRate / 100;
#else
  // Windows.
  static const int kExpectedBufferSize = kHardwareBufferSize;
#endif
  EXPECT_EQ(kExpectedBufferSize, renderer_->frames_per_buffer());
  EXPECT_EQ(kHardwareSampleRate, renderer_->sample_rate());
  EXPECT_EQ(2, renderer_->channels());

  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(WebRtcAudioRendererTest, NonDefaultDevice) {
  SetupRenderer(kDefaultOutputDeviceId);
  EXPECT_EQ(mock_output_device_->GetDeviceId(), kDefaultOutputDeviceId);
  renderer_proxy_->Start();

  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();

  SetupRenderer(kOtherOutputDeviceId);
  EXPECT_EQ(mock_output_device_->GetDeviceId(), kOtherOutputDeviceId);
  renderer_proxy_->Start();

  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(WebRtcAudioRendererTest, SwitchOutputDevice) {
  SetupRenderer(kDefaultOutputDeviceId);
  EXPECT_EQ(kDefaultOutputDeviceId, mock_output_device_->GetDeviceId());
  renderer_proxy_->Start();

  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*this, MockCreateOutputDevice(_, _, kOtherOutputDeviceId, _));
  EXPECT_CALL(*source_.get(), AudioRendererThreadStopped());
  EXPECT_CALL(*this, MockSwitchDeviceCallback(media::OUTPUT_DEVICE_STATUS_OK));
  base::RunLoop loop;
  renderer_proxy_->GetOutputDevice()->SwitchOutputDevice(
      kOtherOutputDeviceId, url::Origin(),
      base::Bind(&WebRtcAudioRendererTest::SwitchDeviceCallback,
                 base::Unretained(this), &loop));
  loop.Run();
  EXPECT_EQ(kOtherOutputDeviceId, mock_output_device_->GetDeviceId());

  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

TEST_F(WebRtcAudioRendererTest, SwitchOutputDeviceInvalidDevice) {
  SetupRenderer(kDefaultOutputDeviceId);
  EXPECT_EQ(kDefaultOutputDeviceId, mock_output_device_->GetDeviceId());
  renderer_proxy_->Start();

  EXPECT_CALL(*this, MockCreateOutputDevice(_, _, kInvalidOutputDeviceId, _));
  EXPECT_CALL(*this, MockSwitchDeviceCallback(
                         media::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL));
  base::RunLoop loop;
  renderer_proxy_->GetOutputDevice()->SwitchOutputDevice(
      kInvalidOutputDeviceId, url::Origin(),
      base::Bind(&WebRtcAudioRendererTest::SwitchDeviceCallback,
                 base::Unretained(this), &loop));
  loop.Run();
  EXPECT_EQ(kDefaultOutputDeviceId, mock_output_device_->GetDeviceId());

  EXPECT_CALL(*mock_output_device_.get(), Stop());
  EXPECT_CALL(*source_.get(), RemoveAudioRenderer(renderer_.get()));
  renderer_proxy_->Stop();
}

}  // namespace content
