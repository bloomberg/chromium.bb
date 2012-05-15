// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "content/renderer/media/mock_peer_connection_impl.h"
#include "content/renderer/media/peer_connection_handler.h"
#include "content/renderer/media/rtc_video_decoder.h"
#include "jingle/glue/thread_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libjingle/source/talk/app/webrtc/peerconnection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamDescriptor.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"

class PeerConnectionHandlerUnderTest : public PeerConnectionHandler {
 public:
  PeerConnectionHandlerUnderTest(
      WebKit::MockWebPeerConnectionHandlerClient* client,
      MediaStreamDependencyFactory* dependency_factory)
      : PeerConnectionHandler(client, dependency_factory) {
  }

  webrtc::MockPeerConnectionImpl* native_peer_connection() {
    return static_cast<webrtc::MockPeerConnectionImpl*>(
        native_peer_connection_.get());
  }
};

class PeerConnectionHandlerTest : public ::testing::Test {
 public:
  PeerConnectionHandlerTest() : mock_peer_connection_(NULL) {
  }

  void SetUp() {
    mock_client_.reset(new WebKit::MockWebPeerConnectionHandlerClient());
    mock_dependency_factory_.reset(
        new MockMediaStreamDependencyFactory(NULL));
    mock_dependency_factory_->CreatePeerConnectionFactory(NULL,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          NULL);
    pc_handler_.reset(
        new PeerConnectionHandlerUnderTest(mock_client_.get(),
                                           mock_dependency_factory_.get()));

    WebKit::WebString server_config(
        WebKit::WebString::fromUTF8("STUN stun.l.google.com:19302"));
    WebKit::WebString username;
    pc_handler_->initialize(server_config, username);

    mock_peer_connection_ = pc_handler_->native_peer_connection();
    ASSERT_TRUE(mock_peer_connection_);
  }

  // Creates a WebKit local MediaStream.
  WebKit::WebMediaStreamDescriptor CreateLocalMediaStream(
      const std::string& stream_label) {
    std::string video_track_label("video-label");
    std::string audio_track_label("audio-label");

    talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> native_stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
        mock_dependency_factory_->CreateLocalAudioTrack(audio_track_label,
                                                        NULL));
    native_stream->AddTrack(audio_track);
    talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
        mock_dependency_factory_->CreateLocalVideoTrack(video_track_label, 0));
    native_stream->AddTrack(video_track);

    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
        static_cast<size_t>(1));
    audio_sources[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                                WebKit::WebMediaStreamSource::TypeAudio,
                                WebKit::WebString::fromUTF8("audio_track"));
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        static_cast<size_t>(1));
    video_sources[0].initialize(WebKit::WebString::fromUTF8(video_track_label),
                                WebKit::WebMediaStreamSource::TypeVideo,
                                WebKit::WebString::fromUTF8("video_track"));
    WebKit::WebMediaStreamDescriptor local_stream;
    local_stream.initialize(UTF8ToUTF16(stream_label), audio_sources,
                            video_sources);
    local_stream.setExtraData(new MediaStreamExtraData(native_stream));
    return local_stream;
  }

  // Creates a remote MediaStream and adds it to the mocked native
  // peer connection.
  talk_base::scoped_refptr<webrtc::MediaStreamInterface>
  AddRemoteMockMediaStream(const std::string& stream_label,
                           const std::string& video_track_label,
                           const std::string& audio_track_label) {
    // We use a local stream as a remote since for testing purposes we really
    // only need the MediaStreamInterface.
    talk_base::scoped_refptr<webrtc::LocalMediaStreamInterface> stream(
        mock_dependency_factory_->CreateLocalMediaStream(stream_label));
    if (!video_track_label.empty()) {
      talk_base::scoped_refptr<webrtc::LocalVideoTrackInterface> video_track(
          mock_dependency_factory_->CreateLocalVideoTrack(video_track_label,
                                                          0));
      stream->AddTrack(video_track);
    }
    if (!audio_track_label.empty()) {
      talk_base::scoped_refptr<webrtc::LocalAudioTrackInterface> audio_track(
          mock_dependency_factory_->CreateLocalAudioTrack(audio_track_label,
                                                          NULL));
      stream->AddTrack(audio_track);
    }
    mock_peer_connection_->AddRemoteStream(stream);
    return stream;
  }

  MessageLoop loop_;
  scoped_ptr<WebKit::MockWebPeerConnectionHandlerClient> mock_client_;
  scoped_ptr<MockMediaStreamDependencyFactory> mock_dependency_factory_;
  scoped_ptr<PeerConnectionHandlerUnderTest> pc_handler_;

  // Weak reference to the mocked native peer connection implementation.
  webrtc::MockPeerConnectionImpl* mock_peer_connection_;
};

TEST_F(PeerConnectionHandlerTest, WebMediaStreamDescriptorMemoryTest) {
  std::string stream_label("stream-label");
  std::string video_track_id("video-label");
  const size_t kSizeOne = 1;

  WebKit::WebMediaStreamSource source;
  source.initialize(WebKit::WebString::fromUTF8(video_track_id),
                    WebKit::WebMediaStreamSource::TypeVideo,
                    WebKit::WebString::fromUTF8("RemoteVideo"));

  WebKit::WebVector<WebKit::WebMediaStreamSource> source_vector(kSizeOne);
  source_vector[0] = source;

  WebKit::WebMediaStreamDescriptor local_stream;
  local_stream.initialize(UTF8ToUTF16(stream_label), source_vector);

  WebKit::WebMediaStreamDescriptor copy_1(local_stream);
  {
    WebKit::WebMediaStreamDescriptor copy_2(copy_1);
  }
}

TEST_F(PeerConnectionHandlerTest, Basic) {
  std::string stream_label("stream-label");
  WebKit::WebVector<WebKit::WebMediaStreamDescriptor> local_streams(
      static_cast<size_t>(1));
  local_streams[0] = CreateLocalMediaStream(stream_label);
  pc_handler_->produceInitialOffer(local_streams);
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_TRUE(mock_peer_connection_->stream_changes_committed());

  std::string message("message1");
  pc_handler_->handleInitialOffer(WebKit::WebString::fromUTF8(message));
  EXPECT_EQ(message, mock_peer_connection_->signaling_message());

  message = "message2";
  pc_handler_->processSDP(WebKit::WebString::fromUTF8(message));
  EXPECT_EQ(message, mock_peer_connection_->signaling_message());

  message = "message3";
  pc_handler_->OnSignalingMessage(message);
  EXPECT_EQ(message, mock_client_->sdp());

  std::string remote_stream_label("remote_stream");
  talk_base::scoped_refptr<webrtc::MediaStreamInterface> remote_stream(
      AddRemoteMockMediaStream(remote_stream_label, "video_track",
                               "audio_track"));
  pc_handler_->OnAddStream(remote_stream);
  EXPECT_EQ(remote_stream_label, mock_client_->stream_label());

  WebKit::WebVector<WebKit::WebMediaStreamDescriptor> empty_streams(
      static_cast<size_t>(0));
  pc_handler_->processPendingStreams(empty_streams, local_streams);
  EXPECT_EQ("", mock_peer_connection_->stream_label());
  mock_peer_connection_->ClearStreamChangesCommitted();
  EXPECT_TRUE(!mock_peer_connection_->stream_changes_committed());

  pc_handler_->OnRemoveStream(remote_stream);
  EXPECT_TRUE(mock_client_->stream_label().empty());

  pc_handler_->processPendingStreams(local_streams, empty_streams);
  EXPECT_EQ(stream_label, mock_peer_connection_->stream_label());
  EXPECT_TRUE(mock_peer_connection_->stream_changes_committed());

  pc_handler_->stop();
  EXPECT_FALSE(pc_handler_->native_peer_connection());
  // PC handler is expected to be deleted when stop calls
  // MediaStreamImpl::ClosePeerConnection. We own and delete it here instead of
  // in the mock.
  pc_handler_.reset();
}
