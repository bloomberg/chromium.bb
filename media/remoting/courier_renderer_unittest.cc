// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/remoting/courier_renderer.h"

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/media_util.h"
#include "media/base/pipeline_status.h"
#include "media/base/renderer_client.h"
#include "media/base/test_helpers.h"
#include "media/remoting/fake_media_resource.h"
#include "media/remoting/fake_remoter.h"
#include "media/remoting/proto_enum_utils.h"
#include "media/remoting/proto_utils.h"
#include "media/remoting/renderer_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Return;

namespace media {
namespace remoting {

namespace {

PipelineMetadata DefaultMetadata() {
  PipelineMetadata data;
  data.has_audio = true;
  data.has_video = true;
  data.video_decoder_config = TestVideoConfig::Normal();
  return data;
}

PipelineStatistics DefaultStats() {
  PipelineStatistics stats;
  stats.audio_bytes_decoded = 1234U;
  stats.video_bytes_decoded = 2345U;
  stats.video_frames_decoded = 3000U;
  stats.video_frames_dropped = 91U;
  stats.audio_memory_usage = 5678;
  stats.video_memory_usage = 6789;
  stats.video_keyframe_distance_average = base::TimeDelta::Max();
  return stats;
}

class RendererClientImpl final : public RendererClient {
 public:
  RendererClientImpl() {
    ON_CALL(*this, OnStatisticsUpdate(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnStatisticsUpdate));
    ON_CALL(*this, OnPipelineStatus(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnPipelineStatus));
    ON_CALL(*this, OnBufferingStateChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnBufferingStateChange));
    ON_CALL(*this, OnAudioConfigChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnAudioConfigChange));
    ON_CALL(*this, OnVideoConfigChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnVideoConfigChange));
    ON_CALL(*this, OnVideoNaturalSizeChange(_))
        .WillByDefault(Invoke(
            this, &RendererClientImpl::DelegateOnVideoNaturalSizeChange));
    ON_CALL(*this, OnVideoOpacityChange(_))
        .WillByDefault(
            Invoke(this, &RendererClientImpl::DelegateOnVideoOpacityChange));
  }
  ~RendererClientImpl() = default;

  // RendererClient implementation.
  void OnError(PipelineStatus status) override {}
  void OnEnded() override {}
  MOCK_METHOD1(OnStatisticsUpdate, void(const PipelineStatistics& stats));
  MOCK_METHOD1(OnBufferingStateChange, void(BufferingState state));
  MOCK_METHOD1(OnAudioConfigChange, void(const AudioDecoderConfig& config));
  MOCK_METHOD1(OnVideoConfigChange, void(const VideoDecoderConfig& config));
  void OnWaiting(WaitingReason reason) override {}
  MOCK_METHOD1(OnVideoNaturalSizeChange, void(const gfx::Size& size));
  MOCK_METHOD1(OnVideoOpacityChange, void(bool opaque));
  MOCK_METHOD1(OnRemotePlayStateChange, void(MediaStatus::State state));

  void DelegateOnStatisticsUpdate(const PipelineStatistics& stats) {
    stats_ = stats;
  }
  void DelegateOnBufferingStateChange(BufferingState state) { state_ = state; }
  void DelegateOnAudioConfigChange(const AudioDecoderConfig& config) {
    audio_decoder_config_ = config;
  }
  void DelegateOnVideoConfigChange(const VideoDecoderConfig& config) {
    video_decoder_config_ = config;
  }
  void DelegateOnVideoNaturalSizeChange(const gfx::Size& size) { size_ = size; }
  void DelegateOnVideoOpacityChange(bool opaque) { opaque_ = opaque; }

  MOCK_METHOD1(OnPipelineStatus, void(PipelineStatus status));
  void DelegateOnPipelineStatus(PipelineStatus status) {
    VLOG(2) << "OnPipelineStatus status:" << status;
    status_ = status;
  }
  MOCK_METHOD0(OnFlushCallback, void());

  PipelineStatus status() const { return status_; }
  PipelineStatistics stats() const { return stats_; }
  BufferingState state() const { return state_; }
  gfx::Size size() const { return size_; }
  bool opaque() const { return opaque_; }
  VideoDecoderConfig video_decoder_config() const {
    return video_decoder_config_;
  }
  AudioDecoderConfig audio_decoder_config() const {
    return audio_decoder_config_;
  }

 private:
  PipelineStatus status_ = PIPELINE_OK;
  BufferingState state_ = BUFFERING_HAVE_NOTHING;
  gfx::Size size_;
  bool opaque_ = false;
  PipelineStatistics stats_;
  VideoDecoderConfig video_decoder_config_;
  AudioDecoderConfig audio_decoder_config_;

  DISALLOW_COPY_AND_ASSIGN(RendererClientImpl);
};

}  // namespace

class CourierRendererTest : public testing::Test {
 public:
  CourierRendererTest()
      : receiver_renderer_handle_(10),
        receiver_audio_demuxer_callback_handle_(11),
        receiver_video_demuxer_callback_handle_(12),
        sender_client_handle_(RpcBroker::kInvalidHandle),
        sender_renderer_callback_handle_(RpcBroker::kInvalidHandle),
        sender_audio_demuxer_handle_(RpcBroker::kInvalidHandle),
        sender_video_demuxer_handle_(RpcBroker::kInvalidHandle),
        received_audio_ds_init_cb_(false),
        received_video_ds_init_cb_(false) {}
  ~CourierRendererTest() override = default;

  // Use this function to mimic receiver to handle RPC message for renderer
  // initialization,
  void RpcMessageResponseBot(std::unique_ptr<std::vector<uint8_t>> message) {
    std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
    ASSERT_TRUE(rpc->ParseFromArray(message->data(), message->size()));
    switch (rpc->proc()) {
      case pb::RpcMessage::RPC_ACQUIRE_RENDERER: {
        // Issues RPC_ACQUIRE_RENDERER_DONE RPC message.
        std::unique_ptr<pb::RpcMessage> acquire_done(new pb::RpcMessage());
        acquire_done->set_handle(rpc->integer_value());
        acquire_done->set_proc(pb::RpcMessage::RPC_ACQUIRE_RENDERER_DONE);
        acquire_done->set_integer_value(receiver_renderer_handle_);
        controller_->GetRpcBroker()->ProcessMessageFromRemote(
            std::move(acquire_done));
      } break;
      case pb::RpcMessage::RPC_R_INITIALIZE: {
        EXPECT_EQ(rpc->handle(), receiver_renderer_handle_);
        sender_renderer_callback_handle_ =
            rpc->renderer_initialize_rpc().callback_handle();
        sender_client_handle_ = rpc->renderer_initialize_rpc().client_handle();
        sender_audio_demuxer_handle_ =
            rpc->renderer_initialize_rpc().audio_demuxer_handle();
        sender_video_demuxer_handle_ =
            rpc->renderer_initialize_rpc().video_demuxer_handle();

        // Issues audio RPC_DS_INITIALIZE RPC message.
        if (sender_audio_demuxer_handle_ != RpcBroker::kInvalidHandle) {
          std::unique_ptr<pb::RpcMessage> ds_init(new pb::RpcMessage());
          ds_init->set_handle(sender_audio_demuxer_handle_);
          ds_init->set_proc(pb::RpcMessage::RPC_DS_INITIALIZE);
          ds_init->set_integer_value(receiver_audio_demuxer_callback_handle_);
          controller_->GetRpcBroker()->ProcessMessageFromRemote(
              std::move(ds_init));
        }
        if (sender_video_demuxer_handle_ != RpcBroker::kInvalidHandle) {
          std::unique_ptr<pb::RpcMessage> ds_init(new pb::RpcMessage());
          ds_init->set_handle(sender_video_demuxer_handle_);
          ds_init->set_proc(pb::RpcMessage::RPC_DS_INITIALIZE);
          ds_init->set_integer_value(receiver_video_demuxer_callback_handle_);
          controller_->GetRpcBroker()->ProcessMessageFromRemote(
              std::move(ds_init));
        }
      } break;
      case pb::RpcMessage::RPC_DS_INITIALIZE_CALLBACK: {
        if (rpc->handle() == receiver_audio_demuxer_callback_handle_)
          received_audio_ds_init_cb_ = true;
        if (rpc->handle() == receiver_video_demuxer_callback_handle_)
          received_video_ds_init_cb_ = true;

        // Issues RPC_R_INITIALIZE_CALLBACK RPC message when receiving
        // RPC_DS_INITIALIZE_CALLBACK on available streams.
        if (received_audio_ds_init_cb_ ==
                (sender_audio_demuxer_handle_ != RpcBroker::kInvalidHandle) &&
            received_video_ds_init_cb_ ==
                (sender_video_demuxer_handle_ != RpcBroker::kInvalidHandle)) {
          std::unique_ptr<pb::RpcMessage> init_cb(new pb::RpcMessage());
          init_cb->set_handle(sender_renderer_callback_handle_);
          init_cb->set_proc(pb::RpcMessage::RPC_R_INITIALIZE_CALLBACK);
          init_cb->set_boolean_value(is_successfully_initialized_);
          controller_->GetRpcBroker()->ProcessMessageFromRemote(
              std::move(init_cb));
        }

      } break;
      case pb::RpcMessage::RPC_R_FLUSHUNTIL: {
        // Issues RPC_R_FLUSHUNTIL_CALLBACK RPC message.
        std::unique_ptr<pb::RpcMessage> flush_cb(new pb::RpcMessage());
        flush_cb->set_handle(rpc->renderer_flushuntil_rpc().callback_handle());
        flush_cb->set_proc(pb::RpcMessage::RPC_R_FLUSHUNTIL_CALLBACK);
        controller_->GetRpcBroker()->ProcessMessageFromRemote(
            std::move(flush_cb));

      } break;

      default:
        NOTREACHED();
    }
    RunPendingTasks();
  }

  // Callback from RpcBroker when sending message to remote sink.
  void OnSendMessageToSink(std::unique_ptr<std::vector<uint8_t>> message) {
    std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
    ASSERT_TRUE(rpc->ParseFromArray(message->data(), message->size()));
    received_rpc_.push_back(std::move(rpc));
  }

 protected:
  void InitializeRenderer() {
    // Register media::RendererClient implementation.
    render_client_.reset(new RendererClientImpl());
    media_resource_.reset(new FakeMediaResource());
    EXPECT_CALL(*render_client_, OnPipelineStatus(_)).Times(1);
    DCHECK(renderer_);
    // Redirect RPC message for simulate receiver scenario
    controller_->GetRpcBroker()->SetMessageCallbackForTesting(base::Bind(
        &CourierRendererTest::RpcMessageResponseBot, base::Unretained(this)));
    RunPendingTasks();
    renderer_->Initialize(media_resource_.get(), render_client_.get(),
                          base::Bind(&RendererClientImpl::OnPipelineStatus,
                                     base::Unretained(render_client_.get())));
    RunPendingTasks();
    // Redirect RPC message back to save for later check.
    controller_->GetRpcBroker()->SetMessageCallbackForTesting(base::Bind(
        &CourierRendererTest::OnSendMessageToSink, base::Unretained(this)));
    RunPendingTasks();
  }

  bool IsRendererInitialized() const {
    return renderer_->state_ == CourierRenderer::STATE_PLAYING;
  }

  bool DidEncounterFatalError() const {
    return renderer_->state_ == CourierRenderer::STATE_ERROR;
  }

  void OnReceivedRpc(std::unique_ptr<pb::RpcMessage> message) {
    renderer_->OnReceivedRpc(std::move(message));
  }

  void SetUp() override {
    controller_ = FakeRemoterFactory::CreateController(false);
    controller_->OnMetadataChanged(DefaultMetadata());

    // Redirect RPC message to CourierRendererTest::OnSendMessageToSink().
    controller_->GetRpcBroker()->SetMessageCallbackForTesting(base::Bind(
        &CourierRendererTest::OnSendMessageToSink, base::Unretained(this)));

    renderer_.reset(new CourierRenderer(base::ThreadTaskRunnerHandle::Get(),
                                        controller_->GetWeakPtr(), nullptr));
    renderer_->clock_ = &clock_;
    clock_.Advance(base::TimeDelta::FromSeconds(1));

    RunPendingTasks();
  }

  CourierRenderer::State state() const { return renderer_->state_; }

  void RunPendingTasks() { base::RunLoop().RunUntilIdle(); }

  // Gets first available RpcMessage with specific |proc|.
  const pb::RpcMessage* PeekRpcMessage(int proc) const {
    for (auto& s : received_rpc_) {
      if (proc == s->proc())
        return s.get();
    }
    return nullptr;
  }
  int ReceivedRpcMessageCount() const { return received_rpc_.size(); }
  void ResetReceivedRpcMessage() { received_rpc_.clear(); }

  void ValidateCurrentTime(base::TimeDelta current,
                           base::TimeDelta current_max) const {
    ASSERT_EQ(renderer_->current_media_time_, current);
    ASSERT_EQ(renderer_->current_max_time_, current_max);
  }

  // Issues RPC_RC_ONTIMEUPDATE RPC message.
  void IssueTimeUpdateRpc(base::TimeDelta media_time,
                          base::TimeDelta max_media_time) {
    std::unique_ptr<remoting::pb::RpcMessage> rpc(
        new remoting::pb::RpcMessage());
    rpc->set_handle(5);
    rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONTIMEUPDATE);
    auto* time_message = rpc->mutable_rendererclient_ontimeupdate_rpc();
    time_message->set_time_usec(media_time.InMicroseconds());
    time_message->set_max_time_usec(max_media_time.InMicroseconds());
    OnReceivedRpc(std::move(rpc));
    RunPendingTasks();
  }

  // Verifies no error reported and issues a series of time updates RPC
  // messages. No verification after the last message is issued.
  void VerifyAndReportTimeUpdates(int start_serial_number,
                                  int end_serial_number) {
    for (int i = start_serial_number; i < end_serial_number; ++i) {
      ASSERT_FALSE(DidEncounterFatalError());
      IssueTimeUpdateRpc(base::TimeDelta::FromMilliseconds(100 + i * 800),
                         base::TimeDelta::FromSeconds(100));
      clock_.Advance(base::TimeDelta::FromSeconds(1));
      RunPendingTasks();
    }
  }

  // Issues RPC_RC_ONSTATISTICSUPDATE RPC message with DefaultStats().
  void IssueStatisticsUpdateRpc() {
    EXPECT_CALL(*render_client_, OnStatisticsUpdate(_)).Times(1);
    const PipelineStatistics stats = DefaultStats();
    std::unique_ptr<remoting::pb::RpcMessage> rpc(
        new remoting::pb::RpcMessage());
    rpc->set_handle(5);
    rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONSTATISTICSUPDATE);
    auto* message = rpc->mutable_rendererclient_onstatisticsupdate_rpc();
    message->set_audio_bytes_decoded(stats.audio_bytes_decoded);
    message->set_video_bytes_decoded(stats.video_bytes_decoded);
    message->set_video_frames_decoded(stats.video_frames_decoded);
    message->set_video_frames_dropped(stats.video_frames_dropped);
    message->set_audio_memory_usage(stats.audio_memory_usage);
    message->set_video_memory_usage(stats.video_memory_usage);
    OnReceivedRpc(std::move(rpc));
    RunPendingTasks();
  }

  // Issue RPC_RC_ONBUFFERINGSTATECHANGE RPC message.
  void IssuesBufferingStateRpc(BufferingState state) {
    base::Optional<pb::RendererClientOnBufferingStateChange::State> pb_state =
        ToProtoMediaBufferingState(state);
    if (!pb_state.has_value())
      return;
    std::unique_ptr<remoting::pb::RpcMessage> rpc(
        new remoting::pb::RpcMessage());
    rpc->set_handle(5);
    rpc->set_proc(remoting::pb::RpcMessage::RPC_RC_ONBUFFERINGSTATECHANGE);
    auto* buffering_state =
        rpc->mutable_rendererclient_onbufferingstatechange_rpc();
    buffering_state->set_state(pb_state.value());
    OnReceivedRpc(std::move(rpc));
    RunPendingTasks();
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<RendererController> controller_;
  std::unique_ptr<RendererClientImpl> render_client_;
  std::unique_ptr<FakeMediaResource> media_resource_;
  std::unique_ptr<CourierRenderer> renderer_;
  base::SimpleTestTickClock clock_;

  // RPC handles.
  const int receiver_renderer_handle_;
  const int receiver_audio_demuxer_callback_handle_;
  const int receiver_video_demuxer_callback_handle_;
  int sender_client_handle_;
  int sender_renderer_callback_handle_;
  int sender_audio_demuxer_handle_;
  int sender_video_demuxer_handle_;

  // Indicate whether RPC_DS_INITIALIZE_CALLBACK RPC messages are received.
  bool received_audio_ds_init_cb_;
  bool received_video_ds_init_cb_;

  // Indicates whether the test wants to simulate successful initialization in
  // the renderer on the receiver side.
  bool is_successfully_initialized_ = true;

  // Stores RPC messages that are sending to remote sink.
  std::vector<std::unique_ptr<pb::RpcMessage>> received_rpc_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CourierRendererTest);
};

TEST_F(CourierRendererTest, Initialize) {
  InitializeRenderer();
  RunPendingTasks();

  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);
}

TEST_F(CourierRendererTest, InitializeFailed) {
  is_successfully_initialized_ = false;
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_FALSE(IsRendererInitialized());
  ASSERT_TRUE(DidEncounterFatalError());
  // Don't report error to prevent breaking the pipeline.
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // The CourierRenderer should act as a no-op renderer from this point.

  ResetReceivedRpcMessage();
  EXPECT_CALL(*render_client_, OnFlushCallback()).Times(1);
  renderer_->Flush(base::Bind(&RendererClientImpl::OnFlushCallback,
                              base::Unretained(render_client_.get())));
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  base::TimeDelta seek = base::TimeDelta::FromMicroseconds(100);
  renderer_->StartPlayingFrom(seek);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  renderer_->SetVolume(3.0);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  renderer_->SetPlaybackRate(2.5);
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());
}

TEST_F(CourierRendererTest, Flush) {
  // Initialize Renderer.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // Flush Renderer.
  // Redirect RPC message for simulate receiver scenario
  controller_->GetRpcBroker()->SetMessageCallbackForTesting(base::Bind(
      &CourierRendererTest::RpcMessageResponseBot, base::Unretained(this)));
  RunPendingTasks();
  EXPECT_CALL(*render_client_, OnFlushCallback()).Times(1);
  renderer_->Flush(base::Bind(&RendererClientImpl::OnFlushCallback,
                              base::Unretained(render_client_.get())));
  RunPendingTasks();
}

TEST_F(CourierRendererTest, StartPlayingFrom) {
  // Initialize Renderer
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_TRUE(IsRendererInitialized());
  ASSERT_EQ(render_client_->status(), PIPELINE_OK);

  // StartPlaying from
  base::TimeDelta seek = base::TimeDelta::FromMicroseconds(100);
  renderer_->StartPlayingFrom(seek);
  RunPendingTasks();

  // Checks if it sends out RPC message with correct value.
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  const pb::RpcMessage* rpc =
      PeekRpcMessage(pb::RpcMessage::RPC_R_STARTPLAYINGFROM);
  ASSERT_TRUE(rpc);
  ASSERT_EQ(rpc->integer64_value(), 100);
}

TEST_F(CourierRendererTest, SetVolume) {
  // Initialize Renderer because, as of this writing, the pipeline guarantees it
  // will not call SetVolume() until after the media::Renderer is initialized.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  // SetVolume() will send pb::RpcMessage::RPC_R_SETVOLUME RPC.
  renderer_->SetVolume(3.0);
  RunPendingTasks();

  // Checks if it sends out RPC message with correct value.
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  const pb::RpcMessage* rpc = PeekRpcMessage(pb::RpcMessage::RPC_R_SETVOLUME);
  ASSERT_TRUE(rpc);
  ASSERT_TRUE(rpc->double_value() == 3.0);
}

TEST_F(CourierRendererTest, SetPlaybackRate) {
  // Initialize Renderer because, as of this writing, the pipeline guarantees it
  // will not call SetPlaybackRate() until after the media::Renderer is
  // initialized.
  InitializeRenderer();
  RunPendingTasks();
  ASSERT_EQ(0, ReceivedRpcMessageCount());

  renderer_->SetPlaybackRate(2.5);
  RunPendingTasks();
  ASSERT_EQ(1, ReceivedRpcMessageCount());
  // Checks if it sends out RPC message with correct value.
  const pb::RpcMessage* rpc =
      PeekRpcMessage(pb::RpcMessage::RPC_R_SETPLAYBACKRATE);
  ASSERT_TRUE(rpc);
  ASSERT_TRUE(rpc->double_value() == 2.5);
}

TEST_F(CourierRendererTest, OnTimeUpdate) {
  base::TimeDelta media_time = base::TimeDelta::FromMicroseconds(100);
  base::TimeDelta max_media_time = base::TimeDelta::FromMicroseconds(500);
  IssueTimeUpdateRpc(media_time, max_media_time);
  ValidateCurrentTime(media_time, max_media_time);

  // Issues RPC_RC_ONTIMEUPDATE RPC message with invalid time
  base::TimeDelta media_time2 = base::TimeDelta::FromMicroseconds(-100);
  base::TimeDelta max_media_time2 = base::TimeDelta::FromMicroseconds(500);
  IssueTimeUpdateRpc(media_time2, max_media_time2);
  // Because of invalid value, the time will not be updated and remain the same.
  ValidateCurrentTime(media_time, max_media_time);
}

TEST_F(CourierRendererTest, OnBufferingStateChange) {
  InitializeRenderer();
  EXPECT_CALL(*render_client_, OnBufferingStateChange(BUFFERING_HAVE_NOTHING))
      .Times(1);
  IssuesBufferingStateRpc(BufferingState::BUFFERING_HAVE_NOTHING);
}

TEST_F(CourierRendererTest, OnAudioConfigChange) {
  const AudioDecoderConfig kNewAudioConfig(kCodecVorbis, kSampleFormatPlanarF32,
                                           CHANNEL_LAYOUT_STEREO, 44100,
                                           EmptyExtraData(), Unencrypted());
  InitializeRenderer();
  // Make sure initial audio config does not match the one we intend to send.
  ASSERT_FALSE(render_client_->audio_decoder_config().Matches(kNewAudioConfig));
  // Issues RPC_RC_ONVIDEOCONFIGCHANGE RPC message.
  EXPECT_CALL(*render_client_,
              OnAudioConfigChange(DecoderConfigEq(kNewAudioConfig)))
      .Times(1);

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONAUDIOCONFIGCHANGE);
  auto* audio_config_change_message =
      rpc->mutable_rendererclient_onaudioconfigchange_rpc();
  pb::AudioDecoderConfig* proto_audio_config =
      audio_config_change_message->mutable_audio_decoder_config();
  ConvertAudioDecoderConfigToProto(kNewAudioConfig, proto_audio_config);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->audio_decoder_config().Matches(kNewAudioConfig));
}

TEST_F(CourierRendererTest, OnVideoConfigChange) {
  const auto kNewVideoConfig = TestVideoConfig::Normal();
  InitializeRenderer();
  // Make sure initial video config does not match the one we intend to send.
  ASSERT_FALSE(render_client_->video_decoder_config().Matches(kNewVideoConfig));
  // Issues RPC_RC_ONVIDEOCONFIGCHANGE RPC message.
  EXPECT_CALL(*render_client_,
              OnVideoConfigChange(DecoderConfigEq(kNewVideoConfig)))
      .Times(1);

  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEOCONFIGCHANGE);
  auto* video_config_change_message =
      rpc->mutable_rendererclient_onvideoconfigchange_rpc();
  pb::VideoDecoderConfig* proto_video_config =
      video_config_change_message->mutable_video_decoder_config();
  ConvertVideoDecoderConfigToProto(kNewVideoConfig, proto_video_config);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->video_decoder_config().Matches(kNewVideoConfig));
}

TEST_F(CourierRendererTest, OnVideoNaturalSizeChange) {
  InitializeRenderer();
  // Makes sure initial value of video natural size is not set to
  // gfx::Size(100, 200).
  ASSERT_NE(render_client_->size().width(), 100);
  ASSERT_NE(render_client_->size().height(), 200);
  // Issues RPC_RC_ONVIDEONATURALSIZECHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoNaturalSizeChange(gfx::Size(100, 200)))
      .Times(1);
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* size_message =
      rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  size_message->set_width(100);
  size_message->set_height(200);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_EQ(render_client_->size().width(), 100);
  ASSERT_EQ(render_client_->size().height(), 200);
}

TEST_F(CourierRendererTest, OnVideoNaturalSizeChangeWithInvalidValue) {
  InitializeRenderer();
  // Issues RPC_RC_ONVIDEONATURALSIZECHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoNaturalSizeChange(_)).Times(0);
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEONATURALSIZECHANGE);
  auto* size_message =
      rpc->mutable_rendererclient_onvideonatualsizechange_rpc();
  size_message->set_width(-100);
  size_message->set_height(0);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
}

TEST_F(CourierRendererTest, OnVideoOpacityChange) {
  InitializeRenderer();
  ASSERT_FALSE(render_client_->opaque());
  // Issues RPC_RC_ONVIDEOOPACITYCHANGE RPC message.
  EXPECT_CALL(*render_client_, OnVideoOpacityChange(true)).Times(1);
  std::unique_ptr<pb::RpcMessage> rpc(new pb::RpcMessage());
  rpc->set_handle(5);
  rpc->set_proc(pb::RpcMessage::RPC_RC_ONVIDEOOPACITYCHANGE);
  rpc->set_boolean_value(true);
  OnReceivedRpc(std::move(rpc));
  RunPendingTasks();
  ASSERT_TRUE(render_client_->opaque());
}

TEST_F(CourierRendererTest, OnStatisticsUpdate) {
  InitializeRenderer();
  EXPECT_NE(DefaultStats(), render_client_->stats());
  IssueStatisticsUpdateRpc();
  EXPECT_EQ(DefaultStats(), render_client_->stats());
}

TEST_F(CourierRendererTest, OnPacingTooSlowly) {
  InitializeRenderer();

  controller_->GetRpcBroker()->SetMessageCallbackForTesting(base::Bind(
      &CourierRendererTest::OnSendMessageToSink, base::Unretained(this)));

  // There should be no error reported with this playback rate.
  renderer_->SetPlaybackRate(0.8);
  RunPendingTasks();
  EXPECT_CALL(*render_client_, OnBufferingStateChange(BUFFERING_HAVE_ENOUGH))
      .Times(1);
  IssuesBufferingStateRpc(BufferingState::BUFFERING_HAVE_ENOUGH);
  clock_.Advance(base::TimeDelta::FromSeconds(3));
  VerifyAndReportTimeUpdates(0, 15);
  ASSERT_FALSE(DidEncounterFatalError());

  // Change playback rate. Pacing keeps same as above. Should report error when
  // playback was continuously delayed for 10 times.
  renderer_->SetPlaybackRate(1);
  RunPendingTasks();
  clock_.Advance(base::TimeDelta::FromSeconds(3));
  VerifyAndReportTimeUpdates(15, 30);
  ASSERT_TRUE(DidEncounterFatalError());
}

TEST_F(CourierRendererTest, OnFrameDropRateHigh) {
  InitializeRenderer();

  for (int i = 0; i < 7; ++i) {
    ASSERT_FALSE(DidEncounterFatalError());  // Not enough measurements.
    IssueStatisticsUpdateRpc();
    clock_.Advance(base::TimeDelta::FromSeconds(1));
    RunPendingTasks();
  }
  ASSERT_TRUE(DidEncounterFatalError());
}

}  // namespace remoting
}  // namespace media
