// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/mock_media_stream_video_source.h"
#include "content/renderer/media/video_track_recorder.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using media::VideoFrame;

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

struct TrackRecorderTestParams {
  const bool use_vp9;
  const size_t first_encoded_frame_size;
  const size_t second_encoded_frame_size;
  const size_t third_encoded_frame_size;
};

const TrackRecorderTestParams kTrackRecorderTestParams[] = {{false, 52, 32, 57},
                                                            {true, 33, 18, 33}};

class VideoTrackRecorderTest : public TestWithParam<TrackRecorderTestParams> {
 public:
  VideoTrackRecorderTest()
      : mock_source_(new MockMediaStreamVideoSource(false)) {
    const blink::WebString webkit_track_id(base::UTF8ToUTF16("dummy"));
    blink_source_.initialize(webkit_track_id,
                             blink::WebMediaStreamSource::TypeVideo,
                             webkit_track_id);
    blink_source_.setExtraData(mock_source_);
    blink_track_.initialize(blink_source_);

    blink::WebMediaConstraints constraints;
    constraints.initialize();
    track_ = new MediaStreamVideoTrack(mock_source_, constraints,
                                       MediaStreamSource::ConstraintsCallback(),
                                       true /* enabled */);
    blink_track_.setExtraData(track_);

    video_track_recorder_.reset(new VideoTrackRecorder(
        GetParam().use_vp9 /* use_vp9 */,
        blink_track_,
        base::Bind(&VideoTrackRecorderTest::OnEncodedVideo,
                   base::Unretained(this))));
    // Paranoia checks.
    EXPECT_EQ(blink_track_.source().extraData(), blink_source_.extraData());
    EXPECT_TRUE(message_loop_.IsCurrent());
  }

  ~VideoTrackRecorderTest() {
    blink_track_.reset();
    blink_source_.reset();
    video_track_recorder_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  MOCK_METHOD4(DoOnEncodedVideo,
               void(const scoped_refptr<VideoFrame>& frame,
                    std::string encoded_data,
                    base::TimeTicks timestamp,
                    bool keyframe));
  void OnEncodedVideo(const scoped_refptr<VideoFrame>& video_frame,
                      scoped_ptr<std::string> encoded_data,
                      base::TimeTicks timestamp,
                      bool is_key_frame) {
    DoOnEncodedVideo(video_frame, *encoded_data, timestamp, is_key_frame);
  }

  void Encode(const scoped_refptr<VideoFrame>& frame,
              base::TimeTicks capture_time) {
    EXPECT_TRUE(message_loop_.IsCurrent());
    video_track_recorder_->OnVideoFrameForTesting(frame, capture_time);
  }

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources below into believing they are on the right threads.
  const base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;

  // All members are non-const due to the series of initialize() calls needed.
  // |mock_source_| is owned by |blink_source_|, |track_| by |blink_track_|.
  MockMediaStreamVideoSource* mock_source_;
  blink::WebMediaStreamSource blink_source_;
  MediaStreamVideoTrack* track_;
  blink::WebMediaStreamTrack blink_track_;

  scoped_ptr<VideoTrackRecorder> video_track_recorder_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoTrackRecorderTest);
};

// Construct and destruct all objects, in particular |video_track_recorder_| and
// its inner object(s). This is a non trivial sequence.
TEST_P(VideoTrackRecorderTest, ConstructAndDestruct) {}

// Creates the encoder and encodes 2 frames of the same size; the encoder should
// be initialised and produce a keyframe, then a non-keyframe. Finally a frame
// of larger size is sent and is expected to be encoded as a keyframe.
TEST_P(VideoTrackRecorderTest, VideoEncoding) {
  // |frame_size| cannot be arbitrarily small, should be reasonable.
  const gfx::Size frame_size(160, 80);
  const scoped_refptr<VideoFrame> video_frame =
      VideoFrame::CreateBlackFrame(frame_size);
  const double kFrameRate = 60.0f;
  video_frame->metadata()->SetDouble(media::VideoFrameMetadata::FRAME_RATE,
                                     kFrameRate);

  InSequence s;
  const base::TimeTicks timeticks_now = base::TimeTicks::Now();
  base::StringPiece first_frame_encoded_data;
  EXPECT_CALL(*this, DoOnEncodedVideo(video_frame, _, timeticks_now, true))
      .Times(1)
      .WillOnce(SaveArg<1>(&first_frame_encoded_data));
  Encode(video_frame, timeticks_now);

  // Send another Video Frame.
  const base::TimeTicks timeticks_later = base::TimeTicks::Now();
  base::StringPiece second_frame_encoded_data;
  EXPECT_CALL(*this, DoOnEncodedVideo(video_frame, _, timeticks_later, false))
      .Times(1)
      .WillOnce(SaveArg<1>(&second_frame_encoded_data));
  Encode(video_frame, timeticks_later);

  // Send another Video Frame and expect only an DoOnEncodedVideo() callback.
  const gfx::Size frame_size2(180, 80);
  const scoped_refptr<VideoFrame> video_frame2 =
      VideoFrame::CreateBlackFrame(frame_size2);

  base::RunLoop run_loop;
  base::Closure quit_closure = run_loop.QuitClosure();

  base::StringPiece third_frame_encoded_data;
  EXPECT_CALL(*this, DoOnEncodedVideo(video_frame2, _, _, true))
      .Times(1)
      .WillOnce(DoAll(SaveArg<1>(&third_frame_encoded_data),
                RunClosure(quit_closure)));
  Encode(video_frame2, base::TimeTicks::Now());

  run_loop.Run();

  EXPECT_EQ(GetParam().first_encoded_frame_size,
            first_frame_encoded_data.size());
  EXPECT_EQ(GetParam().second_encoded_frame_size,
            second_frame_encoded_data.size());
  EXPECT_EQ(GetParam().third_encoded_frame_size,
            third_frame_encoded_data.size());

  Mock::VerifyAndClearExpectations(this);
}

INSTANTIATE_TEST_CASE_P(,
                        VideoTrackRecorderTest,
                        ValuesIn(kTrackRecorderTestParams));

}  // namespace content
