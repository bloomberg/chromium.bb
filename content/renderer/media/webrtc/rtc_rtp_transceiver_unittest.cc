// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_rtp_transceiver.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "content/renderer/media/stream/media_stream_audio_source.h"
#include "content/renderer/media/webrtc/mock_peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/mock_peer_connection_impl.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_adapter_map.h"
#include "content/renderer/media/webrtc/webrtc_media_stream_track_adapter_map.h"
#include "content/renderer/media/webrtc/webrtc_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/web_media_stream_source.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/webrtc/api/test/mock_rtpreceiver.h"
#include "third_party/webrtc/api/test/mock_rtpsender.h"

namespace content {

class RTCRtpTransceiverTest : public ::testing::Test {
 public:
  void SetUp() override {
    dependency_factory_.reset(new MockPeerConnectionDependencyFactory());
    main_task_runner_ = blink::scheduler::GetSingleThreadTaskRunnerForTesting();
    track_map_ = new WebRtcMediaStreamTrackAdapterMap(dependency_factory_.get(),
                                                      main_task_runner_);
    peer_connection_ = new rtc::RefCountedObject<MockPeerConnectionImpl>(
        dependency_factory_.get(), nullptr);
  }

  void TearDown() override {
    // Syncing up with the signaling thread ensures any pending operations on
    // that thread are executed. If they post back to the main thread, such as
    // the sender or receiver destructor traits, this is allowed to execute
    // before the test shuts down the threads.
    SyncWithSignalingThread();
    blink::WebHeap::CollectAllGarbageForTesting();
  }

  // Wait for the signaling thread to perform any queued tasks, executing tasks
  // posted to the current thread in the meantime while waiting.
  void SyncWithSignalingThread() const {
    base::RunLoop run_loop;
    dependency_factory_->GetWebRtcSignalingThread()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner() const {
    return dependency_factory_->GetWebRtcSignalingThread();
  }

  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
  CreateLocalTrackAndAdapter(const std::string& id) {
    return track_map_->GetOrCreateLocalTrackAdapter(CreateBlinkLocalTrack(id));
  }

  std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
  CreateRemoteTrackAndAdapter(const std::string& id) {
    rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track =
        MockWebRtcAudioTrack::Create(id).get();
    std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef> track_ref;
    base::RunLoop run_loop;
    signaling_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCRtpTransceiverTest::CreateRemoteTrackAdapterOnSignalingThread,
            base::Unretained(this), std::move(webrtc_track),
            base::Unretained(&track_ref), base::Unretained(&run_loop)));
    run_loop.Run();
    DCHECK(track_ref);
    return track_ref;
  }

  rtc::scoped_refptr<webrtc::RtpTransceiverInterface> CreateWebRtcTransceiver(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> local_track,
      const std::string& local_stream_id,
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> remote_track,
      const std::string& remote_stream_id) {
    DCHECK_EQ(local_track->kind(), remote_track->kind());
    return new rtc::RefCountedObject<FakeRtpTransceiver>(
        local_track->kind() == webrtc::MediaStreamTrackInterface::kAudioKind
            ? cricket::MEDIA_TYPE_AUDIO
            : cricket::MEDIA_TYPE_VIDEO,
        CreateWebRtcSender(local_track, local_stream_id),
        CreateWebRtcReceiver(remote_track, remote_stream_id));
  }

  rtc::scoped_refptr<webrtc::RtpSenderInterface> CreateWebRtcSender(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::string& stream_id) {
    return new rtc::RefCountedObject<FakeRtpSender>(
        std::move(track), std::vector<std::string>({stream_id}));
  }

  rtc::scoped_refptr<webrtc::RtpReceiverInterface> CreateWebRtcReceiver(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
      const std::string& stream_id) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
        new rtc::RefCountedObject<MockMediaStream>(stream_id));
    return new rtc::RefCountedObject<FakeRtpReceiver>(
        track.get(),
        std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>(
            {remote_stream}));
  }

  RtpTransceiverState CreateTransceiverState(
      rtc::scoped_refptr<webrtc::RtpTransceiverInterface> webrtc_transceiver,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          sender_track_ref,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>
          receiver_track_ref) {
    std::vector<std::string> receiver_stream_ids;
    for (const auto& stream : webrtc_transceiver->receiver()->streams()) {
      receiver_stream_ids.push_back(stream->id());
    }
    return RtpTransceiverState(
        main_task_runner_, signaling_task_runner(), webrtc_transceiver.get(),
        RtpSenderState(main_task_runner_, signaling_task_runner(),
                       webrtc_transceiver->sender().get(),
                       std::move(sender_track_ref),
                       webrtc_transceiver->sender()->stream_ids()),
        RtpReceiverState(main_task_runner_, signaling_task_runner(),
                         webrtc_transceiver->receiver().get(),
                         std::move(receiver_track_ref),
                         std::move(receiver_stream_ids)),
        ToBaseOptional(webrtc_transceiver->mid()),
        webrtc_transceiver->stopped(), webrtc_transceiver->direction(),
        ToBaseOptional(webrtc_transceiver->current_direction()));
  }

 protected:
  blink::WebMediaStreamTrack CreateBlinkLocalTrack(const std::string& id) {
    blink::WebMediaStreamSource web_source;
    web_source.Initialize(
        blink::WebString::FromUTF8(id), blink::WebMediaStreamSource::kTypeAudio,
        blink::WebString::FromUTF8("local_audio_track"), false);
    MediaStreamAudioSource* audio_source = new MediaStreamAudioSource(true);
    // Takes ownership of |audio_source|.
    web_source.SetExtraData(audio_source);

    blink::WebMediaStreamTrack web_track;
    web_track.Initialize(web_source.Id(), web_source);
    audio_source->ConnectToTrack(web_track);
    return web_track;
  }

  void CreateRemoteTrackAdapterOnSignalingThread(
      rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> webrtc_track,
      std::unique_ptr<WebRtcMediaStreamTrackAdapterMap::AdapterRef>* track_ref,
      base::RunLoop* run_loop) {
    *track_ref = track_map_->GetOrCreateRemoteTrackAdapter(webrtc_track.get());
    run_loop->Quit();
  }

 private:
  base::MessageLoop message_loop_;

 protected:
  std::unique_ptr<MockPeerConnectionDependencyFactory> dependency_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<WebRtcMediaStreamTrackAdapterMap> track_map_;
  rtc::scoped_refptr<MockPeerConnectionImpl> peer_connection_;
};

TEST_F(RTCRtpTransceiverTest, InitializeTransceiverState) {
  auto local_track_adapter = CreateLocalTrackAndAdapter("local_track");
  auto remote_track_adapter = CreateRemoteTrackAndAdapter("remote_track");
  auto webrtc_transceiver = CreateWebRtcTransceiver(
      local_track_adapter->webrtc_track(), "local_stream",
      remote_track_adapter->webrtc_track(), "remote_stream");
  RtpTransceiverState transceiver_state =
      CreateTransceiverState(webrtc_transceiver, std::move(local_track_adapter),
                             std::move(remote_track_adapter));
  EXPECT_FALSE(transceiver_state.is_initialized());
  transceiver_state.Initialize();

  EXPECT_TRUE(transceiver_state.is_initialized());
  // Inspect sender states.
  const auto& sender_state = transceiver_state.sender_state();
  EXPECT_TRUE(sender_state);
  EXPECT_TRUE(sender_state->is_initialized());
  const auto& webrtc_sender = webrtc_transceiver->sender();
  EXPECT_EQ(sender_state->webrtc_sender().get(), webrtc_sender.get());
  EXPECT_TRUE(sender_state->track_ref()->is_initialized());
  EXPECT_EQ(sender_state->track_ref()->webrtc_track(),
            webrtc_sender->track().get());
  EXPECT_EQ(sender_state->stream_ids(), webrtc_sender->stream_ids());
  // Inspect receiver states.
  const auto& receiver_state = transceiver_state.receiver_state();
  EXPECT_TRUE(receiver_state);
  EXPECT_TRUE(receiver_state->is_initialized());
  const auto& webrtc_receiver = webrtc_transceiver->receiver();
  EXPECT_EQ(receiver_state->webrtc_receiver().get(), webrtc_receiver.get());
  EXPECT_TRUE(receiver_state->track_ref()->is_initialized());
  EXPECT_EQ(receiver_state->track_ref()->webrtc_track(),
            webrtc_receiver->track().get());
  std::vector<std::string> receiver_stream_ids;
  for (const auto& stream : webrtc_receiver->streams()) {
    receiver_stream_ids.push_back(stream->id());
  }
  EXPECT_EQ(receiver_state->stream_ids(), receiver_stream_ids);
  // Inspect transceiver states.
  EXPECT_TRUE(transceiver_state.mid() == webrtc_transceiver->mid());
  EXPECT_EQ(transceiver_state.stopped(), webrtc_transceiver->stopped());
  EXPECT_TRUE(transceiver_state.direction() == webrtc_transceiver->direction());
  EXPECT_TRUE(transceiver_state.current_direction() ==
              webrtc_transceiver->current_direction());
}

}  // namespace content
