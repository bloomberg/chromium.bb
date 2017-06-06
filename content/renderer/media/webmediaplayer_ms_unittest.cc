// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "content/public/renderer/media_stream_renderer_factory.h"
#include "content/renderer/media/webmediaplayer_ms.h"
#include "content/renderer/media/webmediaplayer_ms_compositor.h"
#include "content/renderer/render_frame_impl.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "third_party/WebKit/public/platform/WebLayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"

namespace content {

enum class FrameType {
  NORMAL_FRAME = 0,
  BROKEN_FRAME = -1,
  TEST_BRAKE = -2,  // Signal to pause message loop.
  MIN_TYPE = TEST_BRAKE
};

using TestFrame = std::pair<FrameType, scoped_refptr<media::VideoFrame>>;

static const int kOddSizeOffset = 3;
static const int kStandardWidth = 320;
static const int kStandardHeight = 240;

class FakeWebMediaPlayerDelegate
    : public media::WebMediaPlayerDelegate,
      public base::SupportsWeakPtr<FakeWebMediaPlayerDelegate> {
 public:
  FakeWebMediaPlayerDelegate() {}
  ~FakeWebMediaPlayerDelegate() override {
    DCHECK(!observer_);
    DCHECK(is_gone_);
  }

  int AddObserver(Observer* observer) override {
    observer_ = observer;
    return delegate_id_;
  }

  void RemoveObserver(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    observer_ = nullptr;
  }

  void DidPlay(int delegate_id,
               bool has_video,
               bool has_audio,
               media::MediaContentType type) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    EXPECT_FALSE(playing_);
    playing_ = true;
    has_video_ = has_video;
    is_gone_ = false;
  }

  void DidPause(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    EXPECT_TRUE(playing_);
    EXPECT_FALSE(is_gone_);
    playing_ = false;
  }

  void PlayerGone(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    is_gone_ = true;
  }

  void SetIdle(int delegate_id, bool is_idle) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    is_idle_ = is_idle;
  }

  bool IsIdle(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    return is_idle_;
  }

  void ClearStaleFlag(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
  }

  bool IsStale(int delegate_id) override {
    EXPECT_EQ(delegate_id_, delegate_id);
    return false;
  }

  void SetIsEffectivelyFullscreen(int delegate_id,
                                  bool is_fullscreen) override {
    EXPECT_EQ(delegate_id_, delegate_id);
  }

  bool IsFrameHidden() override { return is_hidden_; }
  bool IsFrameClosed() override { return false; }

  void set_hidden(bool is_hidden) { is_hidden_ = is_hidden; }

 private:
  int delegate_id_ = 1234;
  Observer* observer_ = nullptr;
  bool playing_ = false;
  bool has_video_ = false;
  bool is_hidden_ = false;
  bool is_gone_ = true;
  bool is_idle_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeWebMediaPlayerDelegate);
};

class ReusableMessageLoopEvent {
 public:
  ReusableMessageLoopEvent() : event_(new media::WaitableMessageLoopEvent()) {}

  base::Closure GetClosure() const { return event_->GetClosure(); }

  media::PipelineStatusCB GetPipelineStatusCB() const {
    return event_->GetPipelineStatusCB();
  }

  void RunAndWait() {
    event_->RunAndWait();
    event_.reset(new media::WaitableMessageLoopEvent());
  }

  void RunAndWaitForStatus(media::PipelineStatus expected) {
    event_->RunAndWaitForStatus(expected);
    event_.reset(new media::WaitableMessageLoopEvent());
  }

 private:
  std::unique_ptr<media::WaitableMessageLoopEvent> event_;
};

// The class is used mainly to inject VideoFrames into WebMediaPlayerMS.
class MockMediaStreamVideoRenderer : public MediaStreamVideoRenderer {
 public:
  MockMediaStreamVideoRenderer(
      const scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      ReusableMessageLoopEvent* message_loop_controller,
      const base::Closure& error_cb,
      const MediaStreamVideoRenderer::RepaintCB& repaint_cb)
      : started_(false),
        task_runner_(task_runner),
        message_loop_controller_(message_loop_controller),
        error_cb_(error_cb),
        repaint_cb_(repaint_cb),
        delay_till_next_generated_frame_(
            base::TimeDelta::FromSecondsD(1.0 / 30.0)) {}

  // Implementation of MediaStreamVideoRenderer
  void Start() override;
  void Stop() override;
  void Resume() override;
  void Pause() override;

  // Methods for test use
  void QueueFrames(const std::vector<int>& timestamps_or_frame_type,
                   bool opaque_frame = true,
                   bool odd_size_frame = false,
                   int double_size_index = -1,
                   media::VideoRotation rotation = media::VIDEO_ROTATION_0);
  bool Started() { return started_; }
  bool Paused() { return paused_; }

 private:
  ~MockMediaStreamVideoRenderer() override {}

  // Main function that pushes a frame into WebMediaPlayerMS
  void InjectFrame();

  // Methods for test use
  void AddFrame(FrameType category,
                const scoped_refptr<media::VideoFrame>& frame);

  bool started_;
  bool paused_;

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ReusableMessageLoopEvent* const message_loop_controller_;
  const base::Closure error_cb_;
  const MediaStreamVideoRenderer::RepaintCB repaint_cb_;

  std::deque<TestFrame> frames_;
  base::TimeDelta delay_till_next_generated_frame_;
};

void MockMediaStreamVideoRenderer::Start() {
  started_ = true;
  paused_ = false;
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&MockMediaStreamVideoRenderer::InjectFrame,
                 base::Unretained(this)));
}

void MockMediaStreamVideoRenderer::Stop() {
  started_ = false;
  frames_.clear();
}

void MockMediaStreamVideoRenderer::Resume() {
  CHECK(started_);
  paused_ = false;
}

void MockMediaStreamVideoRenderer::Pause() {
  CHECK(started_);
  paused_ = true;
}

void MockMediaStreamVideoRenderer::AddFrame(
    FrameType category,
    const scoped_refptr<media::VideoFrame>& frame) {
  frames_.push_back(std::make_pair(category, frame));
}

void MockMediaStreamVideoRenderer::QueueFrames(
    const std::vector<int>& timestamp_or_frame_type,
    bool opaque_frame,
    bool odd_size_frame,
    int double_size_index,
    media::VideoRotation rotation) {
  gfx::Size standard_size = gfx::Size(kStandardWidth, kStandardHeight);
  for (size_t i = 0; i < timestamp_or_frame_type.size(); i++) {
    const int token = timestamp_or_frame_type[i];
    if (static_cast<int>(i) == double_size_index) {
      standard_size = gfx::Size(kStandardWidth * 2, kStandardHeight * 2);
    }
    if (token < static_cast<int>(FrameType::MIN_TYPE)) {
      CHECK(false) << "Unrecognized frame type: " << token;
      return;
    }

    if (token < 0) {
      AddFrame(static_cast<FrameType>(token), nullptr);
      continue;
    }

    if (token >= 0) {
      gfx::Size frame_size;
      if (odd_size_frame) {
        frame_size.SetSize(standard_size.width() - kOddSizeOffset,
                           standard_size.height() - kOddSizeOffset);
      } else {
        frame_size.SetSize(standard_size.width(), standard_size.height());
      }

      auto frame = media::VideoFrame::CreateZeroInitializedFrame(
          opaque_frame ? media::PIXEL_FORMAT_YV12 : media::PIXEL_FORMAT_YV12A,
          frame_size, gfx::Rect(frame_size), frame_size,
          base::TimeDelta::FromMilliseconds(token));

      frame->metadata()->SetRotation(media::VideoFrameMetadata::ROTATION,
                                     rotation);
      frame->metadata()->SetTimeTicks(
          media::VideoFrameMetadata::Key::REFERENCE_TIME,
          base::TimeTicks::Now() + base::TimeDelta::FromMilliseconds(token));

      AddFrame(FrameType::NORMAL_FRAME, frame);
      continue;
    }
  }
}

void MockMediaStreamVideoRenderer::InjectFrame() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (!started_)
    return;

  if (frames_.empty()) {
    message_loop_controller_->GetClosure().Run();
    return;
  }

  auto frame = frames_.front();
  frames_.pop_front();

  if (frame.first == FrameType::BROKEN_FRAME) {
    error_cb_.Run();
    return;
  }

  // For pause case, the provider will still let the stream continue, but
  // not send the frames to the player. As is the same case in reality.
  if (frame.first == FrameType::NORMAL_FRAME) {
    if (!paused_)
      repaint_cb_.Run(frame.second);

    for (size_t i = 0; i < frames_.size(); ++i) {
      if (frames_[i].first == FrameType::NORMAL_FRAME) {
        delay_till_next_generated_frame_ =
            (frames_[i].second->timestamp() - frame.second->timestamp()) /
            (i + 1);
        break;
      }
    }
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MockMediaStreamVideoRenderer::InjectFrame,
                 base::Unretained(this)),
      delay_till_next_generated_frame_);

  // This will pause the |message_loop_|, and the purpose is to allow the main
  // test function to do some operations (e.g. call pause(), switch to
  // background rendering, etc) on WebMediaPlayerMS before resuming
  // |message_loop_|.
  if (frame.first == FrameType::TEST_BRAKE)
    message_loop_controller_->GetClosure().Run();
}

// The class is used to generate a MockVideoProvider in
// WebMediaPlayerMS::load().
class MockRenderFactory : public MediaStreamRendererFactory {
 public:
  MockRenderFactory(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      ReusableMessageLoopEvent* message_loop_controller)
      : task_runner_(task_runner),
        message_loop_controller_(message_loop_controller) {}

  scoped_refptr<MediaStreamVideoRenderer> GetVideoRenderer(
      const blink::WebMediaStream& web_stream,
      const base::Closure& error_cb,
      const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::GpuVideoAcceleratorFactories* gpu_factories) override;

  MockMediaStreamVideoRenderer* provider() {
    return static_cast<MockMediaStreamVideoRenderer*>(provider_.get());
  }

  scoped_refptr<MediaStreamAudioRenderer> GetAudioRenderer(
      const blink::WebMediaStream& web_stream,
      int render_frame_id,
      const std::string& device_id,
      const url::Origin& security_origin) override {
    return nullptr;
  }

 private:
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  scoped_refptr<MediaStreamVideoRenderer> provider_;
  ReusableMessageLoopEvent* const message_loop_controller_;
};

scoped_refptr<MediaStreamVideoRenderer> MockRenderFactory::GetVideoRenderer(
    const blink::WebMediaStream& web_stream,
    const base::Closure& error_cb,
    const MediaStreamVideoRenderer::RepaintCB& repaint_cb,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  provider_ = new MockMediaStreamVideoRenderer(task_runner_,
      message_loop_controller_, error_cb, repaint_cb);

  return provider_;
}

// This is the main class coordinating the tests.
// Basic workflow:
// 1. WebMediaPlayerMS::Load will generate and start
// content::MediaStreamVideoRenderer.
// 2. content::MediaStreamVideoRenderer will start pushing frames into
//    WebMediaPlayerMS repeatedly.
// 3. On WebMediaPlayerMS receiving the first frame, a WebLayer will be created.
// 4. The WebLayer will call
//    WebMediaPlayerMSCompositor::SetVideoFrameProviderClient, which in turn
//    will trigger cc::VideoFrameProviderClient::StartRendering.
// 5. Then cc::VideoFrameProviderClient will start calling
//    WebMediaPlayerMSCompositor::UpdateCurrentFrame, GetCurrentFrame for
//    rendering repeatedly.
// 6. When WebMediaPlayerMS::pause gets called, it should trigger
//    content::MediaStreamVideoRenderer::Pause, and then the provider will stop
//    pushing frames into WebMediaPlayerMS, but instead digesting them;
//    simultanously, it should call cc::VideoFrameProviderClient::StopRendering,
//    so cc::VideoFrameProviderClient will stop asking frames from
//    WebMediaPlayerMSCompositor.
// 7. When WebMediaPlayerMS::play gets called, evething paused in step 6 should
//    be resumed.
class WebMediaPlayerMSTest
    : public testing::TestWithParam<testing::tuple<bool, bool>> ,
      public blink::WebMediaPlayerClient,
      public cc::VideoFrameProvider::Client {
 public:
  WebMediaPlayerMSTest()
      : render_factory_(new MockRenderFactory(message_loop_.task_runner(),
                                              &message_loop_controller_)),
        player_(new WebMediaPlayerMS(
            nullptr,
            this,
            &delegate_,
            base::MakeUnique<media::MediaLog>(),
            std::unique_ptr<MediaStreamRendererFactory>(render_factory_),
            message_loop_.task_runner(),
            message_loop_.task_runner(),
            message_loop_.task_runner(),
            message_loop_.task_runner(),
            nullptr,
            blink::WebString(),
            blink::WebSecurityOrigin())),
        web_layer_set_(false),
        rendering_(false),
        background_rendering_(false) {}
  ~WebMediaPlayerMSTest() override {
    player_.reset();
    base::RunLoop().RunUntilIdle();
  }

  MockMediaStreamVideoRenderer* LoadAndGetFrameProvider(bool algorithm_enabled);

  // Implementation of WebMediaPlayerClient
  void NetworkStateChanged() override;
  void ReadyStateChanged() override;
  void TimeChanged() override {}
  void Repaint() override {}
  void DurationChanged() override {}
  void SizeChanged() override;
  void PlaybackStateChanged() override {}
  void SetWebLayer(blink::WebLayer* layer) override;
  blink::WebMediaPlayer::TrackId AddAudioTrack(const blink::WebString& id,
                                               AudioTrackKind,
                                               const blink::WebString& label,
                                               const blink::WebString& language,
                                               bool enabled) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void RemoveAudioTrack(blink::WebMediaPlayer::TrackId) override {}
  blink::WebMediaPlayer::TrackId AddVideoTrack(const blink::WebString& id,
                                               VideoTrackKind,
                                               const blink::WebString& label,
                                               const blink::WebString& language,
                                               bool selected) override {
    return blink::WebMediaPlayer::TrackId();
  }
  void RemoveVideoTrack(blink::WebMediaPlayer::TrackId) override {}
  void AddTextTrack(blink::WebInbandTextTrack*) override {}
  void RemoveTextTrack(blink::WebInbandTextTrack*) override {}
  void MediaSourceOpened(blink::WebMediaSource*) override {}
  void RequestSeek(double) override {}
  void RemoteRouteAvailabilityChanged(
      blink::WebRemotePlaybackAvailability) override {}
  void ConnectedToRemoteDevice() override {}
  void DisconnectedFromRemoteDevice() override {}
  void CancelledRemotePlaybackRequest() override {}
  void RemotePlaybackStarted() override {}
  void OnBecamePersistentVideo(bool) override {}
  bool IsAutoplayingMuted() override { return false; }
  bool HasSelectedVideoTrack() override { return false; }
  blink::WebMediaPlayer::TrackId GetSelectedVideoTrackId() override {
    return blink::WebMediaPlayer::TrackId();
  }
  bool HasNativeControls() override { return false; }

  // Implementation of cc::VideoFrameProvider::Client
  void StopUsingProvider() override;
  void StartRendering() override;
  void StopRendering() override;
  void DidReceiveFrame() override {}

  // For test use
  void SetBackgroundRendering(bool background_rendering) {
    background_rendering_ = background_rendering;
  }

 protected:
  MOCK_METHOD0(DoStartRendering, void());
  MOCK_METHOD0(DoStopRendering, void());

  MOCK_METHOD1(DoSetWebLayer, void(bool));
  MOCK_METHOD1(DoNetworkStateChanged,
               void(blink::WebMediaPlayer::NetworkState));
  MOCK_METHOD1(DoReadyStateChanged, void(blink::WebMediaPlayer::ReadyState));
  MOCK_METHOD1(CheckSizeChanged, void(gfx::Size));

  base::MessageLoop message_loop_;
  MockRenderFactory* render_factory_;
  FakeWebMediaPlayerDelegate delegate_;
  std::unique_ptr<WebMediaPlayerMS> player_;
  WebMediaPlayerMSCompositor* compositor_;
  ReusableMessageLoopEvent message_loop_controller_;
  blink::WebLayer* web_layer_;

 private:
  // Main function trying to ask WebMediaPlayerMS to submit a frame for
  // rendering.
  void RenderFrame();

  bool web_layer_set_;
  bool rendering_;
  bool background_rendering_;
};

MockMediaStreamVideoRenderer* WebMediaPlayerMSTest::LoadAndGetFrameProvider(
    bool algorithm_enabled) {
  EXPECT_FALSE(!!render_factory_->provider()) << "There should not be a "
                                                 "FrameProvider yet.";

  EXPECT_CALL(*this, DoNetworkStateChanged(
                         blink::WebMediaPlayer::kNetworkStateLoading));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveNothing));
  player_->Load(blink::WebMediaPlayer::kLoadTypeURL,
                blink::WebMediaPlayerSource(),
                blink::WebMediaPlayer::kCORSModeUnspecified);
  compositor_ = player_->compositor_.get();
  EXPECT_TRUE(!!compositor_);
  compositor_->SetAlgorithmEnabledForTesting(algorithm_enabled);

  MockMediaStreamVideoRenderer* const provider = render_factory_->provider();
  EXPECT_TRUE(!!provider);
  EXPECT_TRUE(provider->Started());

  testing::Mock::VerifyAndClearExpectations(this);
  return provider;
}

void WebMediaPlayerMSTest::NetworkStateChanged() {
  blink::WebMediaPlayer::NetworkState state = player_->GetNetworkState();
  DoNetworkStateChanged(state);
  if (state == blink::WebMediaPlayer::NetworkState::kNetworkStateFormatError ||
      state == blink::WebMediaPlayer::NetworkState::kNetworkStateDecodeError ||
      state == blink::WebMediaPlayer::NetworkState::kNetworkStateNetworkError) {
    message_loop_controller_.GetPipelineStatusCB().Run(
        media::PipelineStatus::PIPELINE_ERROR_NETWORK);
  }
}

void WebMediaPlayerMSTest::ReadyStateChanged() {
  blink::WebMediaPlayer::ReadyState state = player_->GetReadyState();
  DoReadyStateChanged(state);
  if (state == blink::WebMediaPlayer::ReadyState::kReadyStateHaveEnoughData)
    player_->Play();
}

void WebMediaPlayerMSTest::SetWebLayer(blink::WebLayer* layer) {
  // Make sure that the old layer is still alive, see http://crbug.com/705448.
  if (web_layer_set_)
    EXPECT_TRUE(web_layer_ != nullptr);
  web_layer_set_ = layer ? true : false;

  web_layer_ = layer;
  if (layer)
    compositor_->SetVideoFrameProviderClient(this);
  DoSetWebLayer(!!layer);
}

void WebMediaPlayerMSTest::StopUsingProvider() {
  if (rendering_)
    StopRendering();
}

void WebMediaPlayerMSTest::StartRendering() {
  if (!rendering_) {
    rendering_ = true;
    message_loop_.task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&WebMediaPlayerMSTest::RenderFrame, base::Unretained(this)));
  }
  DoStartRendering();
}

void WebMediaPlayerMSTest::StopRendering() {
  rendering_ = false;
  DoStopRendering();
}

void WebMediaPlayerMSTest::RenderFrame() {
  if (!rendering_ || !compositor_)
    return;

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeTicks deadline_min =
      now + base::TimeDelta::FromSecondsD(1.0 / 60.0);
  base::TimeTicks deadline_max =
      deadline_min + base::TimeDelta::FromSecondsD(1.0 / 60.0);

  // Background rendering is different from stop rendering. The rendering loop
  // is still running but we do not ask frames from |compositor_|. And
  // background rendering is not initiated from |compositor_|.
  if (!background_rendering_) {
    compositor_->UpdateCurrentFrame(deadline_min, deadline_max);
    auto frame = compositor_->GetCurrentFrame();
    compositor_->PutCurrentFrame();
  }
  message_loop_.task_runner()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&WebMediaPlayerMSTest::RenderFrame, base::Unretained(this)),
      base::TimeDelta::FromSecondsD(1.0 / 60.0));
}

void WebMediaPlayerMSTest::SizeChanged() {
  gfx::Size frame_size = compositor_->GetCurrentSize();
  CheckSizeChanged(frame_size);
}

TEST_F(WebMediaPlayerMSTest, Playing_Normal) {
  // This test sends a bunch of normal frames with increasing timestamps
  // and verifies that they are produced by WebMediaPlayerMS in appropriate
  // order.

  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  int tokens[] = {0,   33,  66,  100, 133, 166, 200, 233, 266, 300,
                  333, 366, 400, 433, 466, 500, 533, 566, 600};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps);

  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  const blink::WebSize& natural_size = player_->NaturalSize();
  EXPECT_EQ(kStandardWidth, natural_size.width);
  EXPECT_EQ(kStandardHeight, natural_size.height);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

TEST_F(WebMediaPlayerMSTest, Playing_ErrorFrame) {
  // This tests sends a broken frame to WebMediaPlayerMS, and verifies
  // OnSourceError function works as expected.

  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(false);

  const int kBrokenFrame = static_cast<int>(FrameType::BROKEN_FRAME);
  int tokens[] = {0,   33,  66,  100, 133, 166, 200, 233, 266, 300,
                  333, 366, 400, 433, 466, 500, 533, 566, 600, kBrokenFrame};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps);

  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this, DoNetworkStateChanged(
                         blink::WebMediaPlayer::kNetworkStateFormatError));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_ERROR_NETWORK);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

TEST_P(WebMediaPlayerMSTest, PlayThenPause) {
  const bool opaque_frame = testing::get<0>(GetParam());
  const bool odd_size_frame = testing::get<1>(GetParam());
  // In the middle of this test, WebMediaPlayerMS::pause will be called, and we
  // are going to verify that during the pause stage, a frame gets freezed, and
  // cc::VideoFrameProviderClient should also be paused.
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(false);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  int tokens[] = {0,   33,  66,  100, 133, kTestBrake, 166, 200, 233, 266,
                  300, 333, 366, 400, 433, 466,        500, 533, 566, 600};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps, opaque_frame, odd_size_frame);

  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  gfx::Size frame_size =
      gfx::Size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));
  EXPECT_CALL(*this, CheckSizeChanged(frame_size));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  // Here we call pause, and expect a freezing frame.
  EXPECT_CALL(*this, DoStopRendering());
  player_->Pause();
  auto prev_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  auto after_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  EXPECT_EQ(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetWebLayer(false));
}

TEST_P(WebMediaPlayerMSTest, PlayThenPauseThenPlay) {
  const bool opaque_frame = testing::get<0>(GetParam());
  const bool odd_size_frame = testing::get<1>(GetParam());
  // Similary to PlayAndPause test above, this one focuses on testing that
  // WebMediaPlayerMS can be resumed after a period of paused status.
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(false);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  int tokens[] = {0,   33,         66,  100, 133, kTestBrake, 166,
                  200, 233,        266, 300, 333, 366,        400,
                  433, kTestBrake, 466, 500, 533, 566,        600};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps, opaque_frame, odd_size_frame);

  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  gfx::Size frame_size =
      gfx::Size(kStandardWidth - (odd_size_frame ? kOddSizeOffset : 0),
                kStandardHeight - (odd_size_frame ? kOddSizeOffset : 0));
  EXPECT_CALL(*this, CheckSizeChanged(frame_size));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  // Here we call pause, and expect a freezing frame.
  EXPECT_CALL(*this, DoStopRendering());
  player_->Pause();
  auto prev_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  auto after_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  EXPECT_EQ(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  // We resume the player, and expect rendering can continue.
  EXPECT_CALL(*this, DoStartRendering());
  player_->Play();
  prev_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  after_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  EXPECT_NE(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

INSTANTIATE_TEST_CASE_P(,
                        WebMediaPlayerMSTest,
                        ::testing::Combine(::testing::Bool(),
                                           ::testing::Bool()));

// During this test, we check that when we send rotated video frames, it applies
// to player's natural size.
TEST_F(WebMediaPlayerMSTest, RotationChange) {
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  static int tokens[] = {0, 33, kTestBrake};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps, false, false, 17, media::VIDEO_ROTATION_90);
  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  blink::WebSize natural_size = player_->NaturalSize();
  // Check that height and width are flipped.
  EXPECT_EQ(kStandardHeight, natural_size.width);
  EXPECT_EQ(kStandardWidth, natural_size.height);

  // Change rotation.
  provider->QueueFrames(timestamps, false, false, 17, media::VIDEO_ROTATION_0);
  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStopRendering());
  EXPECT_CALL(*this, DoStartRendering());
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  natural_size = player_->NaturalSize();
  EXPECT_EQ(kStandardHeight, natural_size.height);
  EXPECT_EQ(kStandardWidth, natural_size.width);

  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

// During this test, we check that web layer changes opacity according to the
// given frames.
TEST_F(WebMediaPlayerMSTest, OpacityChange) {
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  // Push one opaque frame.
  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  static int tokens[] = {0, kTestBrake};
  std::vector<int> timestamps(tokens, tokens + arraysize(tokens));
  provider->QueueFrames(timestamps, true);
  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  ASSERT_TRUE(web_layer_ != nullptr);
  EXPECT_TRUE(web_layer_->Opaque());

  // Push one transparent frame.
  provider->QueueFrames(timestamps, false);
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  EXPECT_FALSE(web_layer_->Opaque());

  // Push another opaque frame.
  provider->QueueFrames(timestamps, true);
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  EXPECT_TRUE(web_layer_->Opaque());

  testing::Mock::VerifyAndClearExpectations(this);
  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

TEST_F(WebMediaPlayerMSTest, BackgroundRendering) {
  // During this test, we will switch to background rendering mode, in which
  // WebMediaPlayerMS::pause does not get called, but
  // cc::VideoFrameProviderClient simply stops asking frames from
  // WebMediaPlayerMS without an explicit notification. We should expect that
  // WebMediaPlayerMS can digest old frames, rather than piling frames up and
  // explode.
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  const int kTestBrake = static_cast<int>(FrameType::TEST_BRAKE);
  int tokens[] = {0,   33,         66,  100, 133, kTestBrake, 166,
                  200, 233,        266, 300, 333, 366,        400,
                  433, kTestBrake, 466, 500, 533, 566,        600};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps);

  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  gfx::Size frame_size = gfx::Size(kStandardWidth, kStandardHeight);
  EXPECT_CALL(*this, CheckSizeChanged(frame_size));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  // Switch to background rendering, expect rendering to continue.
  SetBackgroundRendering(true);
  auto prev_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  auto after_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  EXPECT_NE(prev_frame->timestamp(), after_frame->timestamp());

  // Switch to foreground rendering.
  SetBackgroundRendering(false);
  prev_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  after_frame = compositor_->GetCurrentFrameWithoutUpdatingStatistics();
  EXPECT_NE(prev_frame->timestamp(), after_frame->timestamp());
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

TEST_F(WebMediaPlayerMSTest, FrameSizeChange) {
  // During this test, the frame size of the input changes.
  // We need to make sure, when sizeChanged() gets called, new size should be
  // returned by GetCurrentSize().
  MockMediaStreamVideoRenderer* provider = LoadAndGetFrameProvider(true);

  int tokens[] = {0,   33,  66,  100, 133, 166, 200, 233, 266, 300,
                  333, 366, 400, 433, 466, 500, 533, 566, 600};
  std::vector<int> timestamps(tokens, tokens + sizeof(tokens) / sizeof(int));
  provider->QueueFrames(timestamps, false, false, 7);

  EXPECT_CALL(*this, DoSetWebLayer(true));
  EXPECT_CALL(*this, DoStartRendering());
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveMetadata));
  EXPECT_CALL(*this, DoReadyStateChanged(
                         blink::WebMediaPlayer::kReadyStateHaveEnoughData));
  EXPECT_CALL(*this,
              CheckSizeChanged(gfx::Size(kStandardWidth, kStandardHeight)));
  EXPECT_CALL(*this, CheckSizeChanged(
                         gfx::Size(kStandardWidth * 2, kStandardHeight * 2)));
  message_loop_controller_.RunAndWaitForStatus(
      media::PipelineStatus::PIPELINE_OK);
  testing::Mock::VerifyAndClearExpectations(this);

  EXPECT_CALL(*this, DoSetWebLayer(false));
  EXPECT_CALL(*this, DoStopRendering());
}

#if defined(OS_ANDROID)
TEST_F(WebMediaPlayerMSTest, HiddenPlayerTests) {
  LoadAndGetFrameProvider(true);

  // Hidden status should not affect playback.
  delegate_.set_hidden(true);
  player_->Play();
  EXPECT_FALSE(player_->Paused());

  // A pause delivered via the delegate should not pause the video since these
  // calls are currently ignored.
  player_->OnPause();
  EXPECT_FALSE(player_->Paused());

  // A hidden player should start still be playing upon shown.
  delegate_.set_hidden(false);
  player_->OnFrameShown();
  EXPECT_FALSE(player_->Paused());

  // A hidden event should not pause the player.
  delegate_.set_hidden(true);
  player_->OnFrameHidden();
  EXPECT_FALSE(player_->Paused());

  // A user generated pause() should clear the automatic resumption.
  player_->Pause();
  delegate_.set_hidden(false);
  player_->OnFrameShown();
  EXPECT_TRUE(player_->Paused());

  // A user generated play() should start playback.
  player_->Play();
  EXPECT_FALSE(player_->Paused());

  // An OnSuspendRequested() without forced suspension should do nothing.
  player_->OnIdleTimeout();
  EXPECT_FALSE(player_->Paused());

  // An OnSuspendRequested() with forced suspension should pause playback.
  player_->OnFrameClosed();
  EXPECT_TRUE(player_->Paused());

  // OnShown() should restart after a forced suspension.
  player_->OnFrameShown();
  EXPECT_FALSE(player_->Paused());
  EXPECT_CALL(*this, DoSetWebLayer(false));

  base::RunLoop().RunUntilIdle();
}
#endif

}  // namespace content
