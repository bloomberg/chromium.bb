// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/mock_web_peer_connection_00_handler_client.h"
#include "content/renderer/media/mock_web_peer_connection_handler_client.h"
#include "content/renderer/media/peer_connection_handler_jsep.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/p2p/socket_dispatcher.h"
#include "media/base/message_loop_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamComponent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnection00Handler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPeerConnectionHandler.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"

class MediaStreamImplUnderTest : public MediaStreamImpl {
 public:
  MediaStreamImplUnderTest(MediaStreamDispatcher* media_stream_dispatcher,
                           content::P2PSocketDispatcher* p2p_socket_dispatcher,
                           VideoCaptureImplManager* vc_manager,
                           MediaStreamDependencyFactory* dependency_factory)
      : MediaStreamImpl(NULL, media_stream_dispatcher, p2p_socket_dispatcher,
                        vc_manager, dependency_factory) {
  }

  virtual void CompleteGetUserMediaRequest(
       const WebKit::WebMediaStreamDescriptor& stream,
       WebKit::WebUserMediaRequest* request) {
    last_generated_stream_ = stream;
  }

  virtual WebKit::WebMediaStreamDescriptor GetMediaStream(const GURL& url) {
    return last_generated_stream_;
  }

  const WebKit::WebMediaStreamDescriptor& last_generated_stream() {
    return last_generated_stream_;
  }

 private:
  WebKit::WebMediaStreamDescriptor last_generated_stream_;
};

class MediaStreamImplTest : public ::testing::Test {
 public:
  void SetUp() {
    // Create our test object.
    ms_dispatcher_.reset(new MockMediaStreamDispatcher());
    p2p_socket_dispatcher_ = new content::P2PSocketDispatcher(
        loop_.message_loop_proxy());
    scoped_refptr<VideoCaptureImplManager> vc_manager(
        new VideoCaptureImplManager());
    MockMediaStreamDependencyFactory* dependency_factory =
        new MockMediaStreamDependencyFactory(vc_manager);
    ms_impl_.reset(new MediaStreamImplUnderTest(ms_dispatcher_.get(),
                                                p2p_socket_dispatcher_.get(),
                                                vc_manager.get(),
                                                dependency_factory));
  }

  void TearDown() {
    // Make sure the message created by
    // P2PSocketDispatcher::AsyncMessageSender::Send is handled before
    // tear down to avoid a memory leak.
    loop_.RunAllPending();
  }

  WebKit::WebMediaStreamDescriptor RequestLocalMediaStream(bool audio,
                                                           bool video) {
    WebKit::WebUserMediaRequest user_media_request;
    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
        audio ? static_cast<size_t>(1) : 0);
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        video ? static_cast<size_t>(1) : 0);
    ms_impl_->requestUserMedia(user_media_request, audio_sources,
                               video_sources);

    ms_impl_->OnStreamGenerated(ms_dispatcher_->request_id(),
                                ms_dispatcher_->stream_label(),
                                ms_dispatcher_->audio_array(),
                                ms_dispatcher_->video_array());

    WebKit::WebMediaStreamDescriptor desc = ms_impl_->last_generated_stream();
    MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
        desc.extraData());
    if (!extra_data || !extra_data->local_stream()) {
      ADD_FAILURE();
      return desc;
    }

    if (audio)
      EXPECT_EQ(1u, extra_data->local_stream()->audio_tracks()->count());
    if (video)
      EXPECT_EQ(1u, extra_data->local_stream()->video_tracks()->count());
    if (audio && video) {
      EXPECT_NE(extra_data->local_stream()->audio_tracks()->at(0)->label(),
                extra_data->local_stream()->video_tracks()->at(0)->label());
    }
    return desc;
  }

 protected:
  MessageLoop loop_;
  scoped_ptr<MockMediaStreamDispatcher> ms_dispatcher_;
  scoped_refptr<content::P2PSocketDispatcher> p2p_socket_dispatcher_;
  scoped_ptr<MediaStreamImplUnderTest> ms_impl_;
};

TEST_F(MediaStreamImplTest, CreatePeerConnection) {
  // Create JSEP PeerConnection.
  WebKit::MockWebPeerConnection00HandlerClient client_jsep;
  scoped_ptr<WebKit::WebPeerConnection00Handler> pc_handler_jsep(
      ms_impl_->CreatePeerConnectionHandlerJsep(&client_jsep));
  pc_handler_jsep.reset();
}

TEST_F(MediaStreamImplTest, LocalMediaStream) {
  // Test a stream with both audio and video.
  WebKit::WebMediaStreamDescriptor mixed_desc = RequestLocalMediaStream(true,
                                                                        true);
  // Create a renderer for the stream.
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactory());
  scoped_refptr<media::VideoDecoder> mixed_decoder(
      ms_impl_->GetVideoDecoder(GURL(), message_loop_factory.get()));
  EXPECT_TRUE(mixed_decoder.get() != NULL);

  // Test a stream with audio only.
  WebKit::WebMediaStreamDescriptor audio_desc = RequestLocalMediaStream(true,
                                                                        false);
  scoped_refptr<media::VideoDecoder> audio_decoder(
      ms_impl_->GetVideoDecoder(GURL(), message_loop_factory.get()));
  EXPECT_TRUE(audio_decoder.get() == NULL);

  // Test a stream with video only.
  WebKit::WebMediaStreamDescriptor video_desc = RequestLocalMediaStream(false,
                                                                        true);
  scoped_refptr<media::VideoDecoder> video_decoder(
      ms_impl_->GetVideoDecoder(GURL(), message_loop_factory.get()));
  EXPECT_TRUE(video_decoder.get() != NULL);

  // Stop generated local streams.
  ms_impl_->StopLocalMediaStream(mixed_desc);
  EXPECT_EQ(1, ms_dispatcher_->stop_stream_counter());
  ms_impl_->StopLocalMediaStream(audio_desc);
  EXPECT_EQ(2, ms_dispatcher_->stop_stream_counter());

  // Test that the MediaStreams are deleted if the owning WebFrame is deleted.
  // In the unit test the owning frame is NULL.
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(3, ms_dispatcher_->stop_stream_counter());
}

TEST_F(MediaStreamImplTest, CloneLocalMediaStream) {
  // Request a stream with both audio and video.
  WebKit::WebMediaStreamDescriptor mixed_desc =
      RequestLocalMediaStream(true, true);

  // Create a WebKit MediaStream description with the same sources as the
  // sources used in the original requested media stream.
  WebKit::WebMediaStreamDescriptor cloned_desc;
  WebKit::WebVector<WebKit::WebMediaStreamComponent> compontents;
  mixed_desc.audioSources(compontents);
  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
      static_cast<size_t>(1));
  audio_sources[0] = compontents[0].source();
  mixed_desc.videoSources(compontents);
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
      static_cast<size_t>(1));
  video_sources[0] = compontents[0].source();
  cloned_desc.initialize("cloned stream", audio_sources, video_sources);

  // Test that we can clone the local MediaStream.
  ms_impl_->CreateMediaStream(NULL, &cloned_desc);

  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      mixed_desc.extraData());

  MediaStreamExtraData* cloned_extra_data = static_cast<MediaStreamExtraData*>(
      cloned_desc.extraData());

  ASSERT_TRUE(extra_data && extra_data->local_stream());
  ASSERT_TRUE(cloned_extra_data && cloned_extra_data->local_stream());
  ASSERT_EQ(1u, extra_data->local_stream()->video_tracks()->count());
  ASSERT_EQ(1u, extra_data->local_stream()->audio_tracks()->count());

  EXPECT_EQ(1u, cloned_extra_data->local_stream()->video_tracks()->count());
  EXPECT_EQ(1u, cloned_extra_data->local_stream()->audio_tracks()->count());
  EXPECT_NE(extra_data->local_stream()->label(),
            cloned_extra_data->local_stream()->label());
}

// Test that we don't crash if a MediaStream is created in WebKit with unknown
// sources. This can for example happen if a MediaStream is created with
// remote tracks.
TEST_F(MediaStreamImplTest, CreateInvalidMediaStream) {
  // Create a WebKit MediaStream description.
  WebKit::WebMediaStreamDescriptor desc;
  WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
      static_cast<size_t>(1));
  audio_sources[0].initialize("audio source",
                              WebKit::WebMediaStreamSource::TypeAudio,
                              "something");
  WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
      static_cast<size_t>(1));
  video_sources[0].initialize("video source",
                              WebKit::WebMediaStreamSource::TypeVideo,
                              "something");
  desc.initialize("new stream", audio_sources, video_sources);

  ms_impl_->CreateMediaStream(NULL, &desc);
  MediaStreamExtraData* extra_data = static_cast<MediaStreamExtraData*>(
      desc.extraData());

  ASSERT_TRUE(extra_data && extra_data->local_stream());
  EXPECT_EQ(0u, extra_data->local_stream()->video_tracks()->count());
  EXPECT_EQ(0u, extra_data->local_stream()->audio_tracks()->count());
}
