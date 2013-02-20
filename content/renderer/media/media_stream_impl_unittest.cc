// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "content/renderer/media/media_stream_extra_data.h"
#include "content/renderer/media/media_stream_impl.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_dispatcher.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStream.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamSource.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebMediaStreamTrack.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"

namespace content {

class MediaStreamImplUnderTest : public MediaStreamImpl {
 public:
  enum RequestState {
    REQUEST_NOT_STARTED,
    REQUEST_NOT_COMPLETE,
    REQUEST_SUCCEEDED,
    REQUEST_FAILED,
  };

  MediaStreamImplUnderTest(MediaStreamDispatcher* media_stream_dispatcher,
                           VideoCaptureImplManager* vc_manager,
                           MediaStreamDependencyFactory* dependency_factory)
      : MediaStreamImpl(NULL, media_stream_dispatcher, vc_manager,
                        dependency_factory),
        state_(REQUEST_NOT_STARTED) {
  }

  void RequestUserMedia(bool audio, bool video) {
    WebKit::WebUserMediaRequest user_media_request;
    WebKit::WebVector<WebKit::WebMediaStreamSource> audio_sources(
        audio ? static_cast<size_t>(1) : 0);
    WebKit::WebVector<WebKit::WebMediaStreamSource> video_sources(
        video ? static_cast<size_t>(1) : 0);
    state_ = REQUEST_NOT_COMPLETE;
    requestUserMedia(user_media_request, audio_sources,
                     video_sources);
  }

  virtual void CompleteGetUserMediaRequest(
      const WebKit::WebMediaStream& stream,
      WebKit::WebUserMediaRequest* request_info,
      bool request_succeeded) OVERRIDE {
    last_generated_stream_ = stream;
    state_ = request_succeeded ? REQUEST_SUCCEEDED : REQUEST_FAILED;
  }

  virtual WebKit::WebMediaStream GetMediaStream(
      const GURL& url) OVERRIDE {
    return last_generated_stream_;
  }

  using MediaStreamImpl::OnLocalMediaStreamStop;

  const WebKit::WebMediaStream& last_generated_stream() {
    return last_generated_stream_;
  }

  RequestState request_state() const { return state_; }

 private:
  WebKit::WebMediaStream last_generated_stream_;
  RequestState state_;
};

class MediaStreamImplTest : public ::testing::Test {
 public:
  virtual void SetUp() {
    // Create our test object.
    ms_dispatcher_.reset(new MockMediaStreamDispatcher());
    scoped_refptr<VideoCaptureImplManager> vc_manager(
        new VideoCaptureImplManager());
    dependency_factory_.reset(new MockMediaStreamDependencyFactory());
    ms_impl_.reset(new MediaStreamImplUnderTest(ms_dispatcher_.get(),
                                                vc_manager.get(),
                                                dependency_factory_.get()));
  }

  WebKit::WebMediaStream RequestLocalMediaStream(bool audio,
                                                           bool video) {
    ms_impl_->RequestUserMedia(audio, video);
    FakeMediaStreamDispatcherComplete();
    if (video)
      ChangeVideoSourceStateToLive();
    if (audio)
      ChangeAudioSourceStateToLive();

    EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_SUCCEEDED,
              ms_impl_->request_state());

    WebKit::WebMediaStream desc = ms_impl_->last_generated_stream();
    content::MediaStreamExtraData* extra_data =
        static_cast<content::MediaStreamExtraData*>(desc.extraData());
    if (!extra_data || !extra_data->stream()) {
      ADD_FAILURE();
      return desc;
    }

    if (audio)
      EXPECT_EQ(1u, extra_data->stream()->GetAudioTracks().size());
    if (video)
      EXPECT_EQ(1u, extra_data->stream()->GetVideoTracks().size());
    if (audio && video) {
      EXPECT_NE(extra_data->stream()->GetAudioTracks()[0]->id(),
                extra_data->stream()->GetVideoTracks()[0]->id());
    }
    return desc;
  }


  void FakeMediaStreamDispatcherComplete() {
    ms_impl_->OnStreamGenerated(ms_dispatcher_->request_id(),
                                ms_dispatcher_->stream_label(),
                                ms_dispatcher_->audio_array(),
                                ms_dispatcher_->video_array());
  }

  void ChangeVideoSourceStateToLive() {
    if (dependency_factory_->last_video_source() != NULL) {
      dependency_factory_->last_video_source()->SetLive();
    }
  }

  void ChangeAudioSourceStateToLive() {
    if (dependency_factory_->last_audio_source() != NULL) {
      dependency_factory_->last_audio_source()->SetLive();
    }
  }

  void ChangeVideoSourceStateToEnded() {
    if (dependency_factory_->last_video_source() != NULL) {
      dependency_factory_->last_video_source()->SetEnded();
    }
  }

  void ChangeAudioSourceStateToEnded() {
    if (dependency_factory_->last_audio_source() != NULL) {
      dependency_factory_->last_audio_source()->SetEnded();
    }
  }

 protected:
  scoped_ptr<MockMediaStreamDispatcher> ms_dispatcher_;
  scoped_ptr<MediaStreamImplUnderTest> ms_impl_;
  scoped_ptr<MockMediaStreamDependencyFactory> dependency_factory_;
};

TEST_F(MediaStreamImplTest, LocalMediaStream) {
  // Test a stream with both audio and video.
  WebKit::WebMediaStream mixed_desc = RequestLocalMediaStream(true, true);
  // Create a renderer for the stream.
  scoped_refptr<media::VideoDecoder> mixed_decoder(
      ms_impl_->GetVideoDecoder(GURL(), base::MessageLoopProxy::current()));
  EXPECT_TRUE(mixed_decoder.get() != NULL);

  // Test a stream with audio only.
  WebKit::WebMediaStream audio_desc = RequestLocalMediaStream(true, false);
  scoped_refptr<media::VideoDecoder> audio_decoder(
      ms_impl_->GetVideoDecoder(GURL(), base::MessageLoopProxy::current()));
  EXPECT_TRUE(audio_decoder.get() == NULL);

  // Test a stream with video only.
  WebKit::WebMediaStream video_desc = RequestLocalMediaStream(false, true);
  scoped_refptr<media::VideoDecoder> video_decoder(
      ms_impl_->GetVideoDecoder(GURL(), base::MessageLoopProxy::current()));
  EXPECT_TRUE(video_decoder.get() != NULL);

  // Stop generated local streams.
  ms_impl_->OnLocalMediaStreamStop(mixed_desc.label().utf8());
  EXPECT_EQ(1, ms_dispatcher_->stop_stream_counter());
  ms_impl_->OnLocalMediaStreamStop(audio_desc.label().utf8());
  EXPECT_EQ(2, ms_dispatcher_->stop_stream_counter());

  // Test that the MediaStreams are deleted if the owning WebFrame is deleted.
  // In the unit test the owning frame is NULL.
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(3, ms_dispatcher_->stop_stream_counter());
}

// This test what happens if a source to a MediaSteam fails to start.
TEST_F(MediaStreamImplTest, MediaSourceFailToStart) {
  ms_impl_->RequestUserMedia(true, true);
  FakeMediaStreamDispatcherComplete();
  ChangeVideoSourceStateToEnded();
  ChangeAudioSourceStateToEnded();
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_FAILED,
            ms_impl_->request_state());
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->stop_stream_counter());
}

// This test what happens if MediaStreamImpl is deleted while the sources of a
// MediaStream is being started.
TEST_F(MediaStreamImplTest, MediaStreamImplShutDown) {
  ms_impl_->RequestUserMedia(true, true);
  FakeMediaStreamDispatcherComplete();
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_NOT_COMPLETE,
            ms_impl_->request_state());
  ms_impl_.reset();
  ChangeAudioSourceStateToLive();
  ChangeVideoSourceStateToLive();
}

// This test what happens if the WebFrame is closed while the MediaStream is
// being generated by the MediaStreamDispatcher.
TEST_F(MediaStreamImplTest, ReloadFrameWhileGeneratingStream) {
  ms_impl_->RequestUserMedia(true, true);
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  EXPECT_EQ(0, ms_dispatcher_->stop_stream_counter());
  ChangeAudioSourceStateToLive();
  ChangeVideoSourceStateToLive();
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_NOT_COMPLETE,
            ms_impl_->request_state());
}

// This test what happens if the WebFrame is closed while the sources are being
// started by MediaStreamDependencyFactory.
TEST_F(MediaStreamImplTest, ReloadFrameWhileGeneratingSources) {
  ms_impl_->RequestUserMedia(true, true);
  FakeMediaStreamDispatcherComplete();
  EXPECT_EQ(0, ms_dispatcher_->stop_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->stop_stream_counter());
  ChangeAudioSourceStateToLive();
  ChangeVideoSourceStateToLive();
  EXPECT_EQ(MediaStreamImplUnderTest::REQUEST_NOT_COMPLETE,
            ms_impl_->request_state());
}

// This test what happens if stop is called on a stream after the frame has
// been reloaded.
TEST_F(MediaStreamImplTest, StopStreamAfterReload) {
  WebKit::WebMediaStream mixed_desc = RequestLocalMediaStream(true, true);
  EXPECT_EQ(0, ms_dispatcher_->stop_stream_counter());
  EXPECT_EQ(1, ms_dispatcher_->request_stream_counter());
  ms_impl_->FrameWillClose(NULL);
  EXPECT_EQ(1, ms_dispatcher_->stop_stream_counter());
  ms_impl_->OnLocalMediaStreamStop(mixed_desc.label().utf8());
  EXPECT_EQ(1, ms_dispatcher_->stop_stream_counter());
}

}  // namespace content
