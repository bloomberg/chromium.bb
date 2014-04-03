// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/media/media_stream.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_dependency_factory.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "content/renderer/media/mock_media_stream_video_sink.h"
#include "content/renderer/media/webrtc/video_destination_handler.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppb_image_data_impl.h"
#include "content/test/ppapi_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaStreamTrack.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {

static const std::string kTestStreamUrl = "stream_url";
static const std::string kUnknownStreamUrl = "unknown_stream_url";

class VideoDestinationHandlerTest : public PpapiUnittest {
 public:
  VideoDestinationHandlerTest() : registry_(&factory_) {
    registry_.Init(kTestStreamUrl);
  }

 protected:
  MockMediaStreamDependencyFactory factory_;
  MockMediaStreamRegistry registry_;
};

TEST_F(VideoDestinationHandlerTest, Open) {
  FrameWriterInterface* frame_writer = NULL;
  // Unknow url will return false.
  EXPECT_FALSE(VideoDestinationHandler::Open(&factory_, &registry_,
                                             kUnknownStreamUrl, &frame_writer));
  EXPECT_TRUE(VideoDestinationHandler::Open(&factory_, &registry_,
                                            kTestStreamUrl, &frame_writer));
  // The |frame_writer| is a proxy and is owned by who call Open.
  delete frame_writer;
}

TEST_F(VideoDestinationHandlerTest, PutFrame) {
  FrameWriterInterface* frame_writer = NULL;
  EXPECT_TRUE(VideoDestinationHandler::Open(&factory_, &registry_,
                                            kTestStreamUrl, &frame_writer));
  ASSERT_TRUE(frame_writer);

  // Verify the video track has been added.
  const blink::WebMediaStream test_stream = registry_.test_stream();
  blink::WebVector<blink::WebMediaStreamTrack> video_tracks;
  test_stream.videoTracks(video_tracks);
  ASSERT_EQ(1u, video_tracks.size());

  // Verify the native video track has been added.
  MediaStreamVideoTrack* native_track =
      MediaStreamVideoTrack::GetVideoTrack(video_tracks[0]);
  ASSERT_TRUE(native_track != NULL);

  MockMediaStreamVideoSink sink;
  native_track->AddSink(&sink);

  scoped_refptr<PPB_ImageData_Impl> image(
      new PPB_ImageData_Impl(instance()->pp_instance(),
                             PPB_ImageData_Impl::ForTest()));
  image->Init(PP_IMAGEDATAFORMAT_BGRA_PREMUL, 640, 360, true);
  frame_writer->PutFrame(image, 10);
  EXPECT_EQ(1, sink.number_of_frames());
  // TODO(perkj): Verify that the track output I420 when
  // https://codereview.chromium.org/213423006/ is landed.

  native_track->RemoveSink(&sink);

  // The |frame_writer| is a proxy and is owned by who call Open.
  delete frame_writer;
}

}  // namespace content
