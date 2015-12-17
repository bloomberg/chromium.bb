// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "content/child/child_process.h"
#include "content/renderer/media/media_recorder_handler.h"
#include "content/renderer/media/mock_media_stream_registry.h"
#include "media/audio/simple_sources.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebMediaRecorderHandlerClient.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebHeap.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::InSequence;
using ::testing::Lt;
using ::testing::Mock;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

using blink::WebString;

namespace content {

ACTION_P(RunClosure, closure) {
  closure.Run();
}

static const std::string kTestStreamUrl = "stream_url";
static const std::string kTestVideoTrackId = "video_track_id";
static const std::string kTestAudioTrackId = "audio_track_id";
static const int kTestAudioChannels = 2;
static const int kTestAudioBitsPerSample = 16;
static const int kTestAudioSampleRate = 48000;
static const int kTestAudioBufferDurationMS = 60;

struct MediaRecorderTestParams {
  const bool has_video;
  const bool has_audio;
  const char* const mime_type;
  const size_t first_encoded_video_frame_size;
  const size_t second_encoded_video_frame_size;
  const size_t first_encoded_audio_frame_size;
  const size_t second_encoded_audio_frame_size;
};

static const MediaRecorderTestParams kMediaRecorderTestParams[] = {
    {true, false, "video/vp8", 52, 32, 0, 0},
    {true, false, "video/vp9", 33, 18, 0, 0},
    {false, true, "video/vp8", 0, 0, 990, 706}};

class MediaRecorderHandlerTest : public TestWithParam<MediaRecorderTestParams>,
                                 public blink::WebMediaRecorderHandlerClient {
 public:
  MediaRecorderHandlerTest()
      : media_recorder_handler_(new MediaRecorderHandler()),
        audio_source_(kTestAudioChannels,
                      440 /* freq */,
                      kTestAudioSampleRate) {
    EXPECT_FALSE(media_recorder_handler_->recording_);

    registry_.Init(kTestStreamUrl);
  }

  ~MediaRecorderHandlerTest() {
    registry_.reset();
    blink::WebHeap::collectAllGarbageForTesting();
  }

  MOCK_METHOD3(writeData, void(const char*, size_t, bool));
  MOCK_METHOD1(failOutOfMemory, void(const WebString& message));
  MOCK_METHOD1(failIllegalStreamModification, void(const WebString& message));
  MOCK_METHOD1(failOtherRecordingError, void(const WebString& message));

  bool recording() const { return media_recorder_handler_->recording_; }
  bool hasVideoRecorders() const {
    return !media_recorder_handler_->video_recorders_.empty();
  }
  bool hasAudioRecorders() const {
    return !media_recorder_handler_->audio_recorders_.empty();
  }

  void OnVideoFrameForTesting(const scoped_refptr<media::VideoFrame>& frame) {
    media_recorder_handler_->OnVideoFrameForTesting(frame,
                                                    base::TimeTicks::Now());
  }
  void OnAudioBusForTesting(const media::AudioBus& audio_bus) {
    media_recorder_handler_->OnAudioBusForTesting(audio_bus,
                                                  base::TimeTicks::Now());
  }
  void SetAudioFormatForTesting(const media::AudioParameters& params) {
    media_recorder_handler_->SetAudioFormatForTesting(params);
  }

  void AddTracks() {
    // Avoid issues with non-parameterized tests by calling this outside of ctr.
    if (GetParam().has_video)
      registry_.AddVideoTrack(kTestVideoTrackId);
    if (GetParam().has_audio)
      registry_.AddAudioTrack(kTestAudioTrackId);
  }

  scoped_ptr<media::AudioBus> NextAudioBus() {
    scoped_ptr<media::AudioBus> bus(media::AudioBus::Create(
        kTestAudioChannels,
        kTestAudioSampleRate * kTestAudioBufferDurationMS / 1000));
    audio_source_.OnMoreData(bus.get(), 0);
    return bus.Pass();
  }

  // A ChildProcess and a MessageLoopForUI are both needed to fool the Tracks
  // and Sources in |registry_| into believing they are on the right threads.
  const base::MessageLoopForUI message_loop_;
  const ChildProcess child_process_;
  MockMediaStreamRegistry registry_;

  // The Class under test. Needs to be scoped_ptr to force its destruction.
  scoped_ptr<MediaRecorderHandler> media_recorder_handler_;

  // For generating test AudioBuses
  media::SineWaveAudioSource audio_source_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaRecorderHandlerTest);
};

// Checks that canSupportMimeType() works as expected.
// TODO(mcasas): revisit this when canSupportMimeType() is fully implemented.
TEST_F(MediaRecorderHandlerTest, CanSupportMimeType) {
  const WebString good_mime_type_vp8(base::UTF8ToUTF16("video/vp8"));
  EXPECT_TRUE(media_recorder_handler_->canSupportMimeType(good_mime_type_vp8));

  const WebString good_mime_type_vp9(base::UTF8ToUTF16("video/vp9"));
  EXPECT_TRUE(media_recorder_handler_->canSupportMimeType(good_mime_type_vp9));

  const WebString audio_mime_type(base::UTF8ToUTF16("audio/opus"));
  EXPECT_TRUE(media_recorder_handler_->canSupportMimeType(audio_mime_type));

  const WebString combo_mime_type(base::UTF8ToUTF16("video/vp8, audio/opus"));
  EXPECT_TRUE(media_recorder_handler_->canSupportMimeType(combo_mime_type));

  const WebString bad_combo(base::UTF8ToUTF16("video/vp8, audio/unsupported"));
  EXPECT_FALSE(media_recorder_handler_->canSupportMimeType(bad_combo));

  const WebString bad_mime_type(base::UTF8ToUTF16("video/unsupportedcodec"));
  EXPECT_FALSE(media_recorder_handler_->canSupportMimeType(bad_mime_type));

  const WebString empty_mime_type(base::UTF8ToUTF16(""));
  EXPECT_TRUE(media_recorder_handler_->canSupportMimeType(empty_mime_type));
}

// Checks that the initialization-destruction sequence works fine.
TEST_P(MediaRecorderHandlerTest, InitializeStartStop) {
  AddTracks();
  const WebString mime_type(base::UTF8ToUTF16(GetParam().mime_type));
  EXPECT_TRUE(media_recorder_handler_->initialize(this,
                                                  registry_.test_stream(),
                                                  mime_type));
  EXPECT_FALSE(recording());
  EXPECT_FALSE(hasVideoRecorders());
  EXPECT_FALSE(hasAudioRecorders());

  EXPECT_TRUE(media_recorder_handler_->start());
  EXPECT_TRUE(recording());

  EXPECT_TRUE(hasVideoRecorders() || !GetParam().has_video);
  EXPECT_TRUE(hasAudioRecorders() || !GetParam().has_audio);

  media_recorder_handler_->stop();
  EXPECT_FALSE(recording());
  EXPECT_FALSE(hasVideoRecorders());
  EXPECT_FALSE(hasAudioRecorders());

  // Expect a last call on destruction.
  EXPECT_CALL(*this, writeData(_, _, true)).Times(1);
  media_recorder_handler_.reset();
}

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_P(MediaRecorderHandlerTest, EncodeVideoFrames) {
  // Video-only test.
  if (GetParam().has_audio)
    return;

  AddTracks();

  const WebString mime_type(base::UTF8ToUTF16(GetParam().mime_type));
  EXPECT_TRUE(media_recorder_handler_->initialize(this, registry_.test_stream(),
                                                  mime_type));
  EXPECT_TRUE(media_recorder_handler_->start());

  InSequence s;
  const scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateBlackFrame(gfx::Size(160, 80));

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    // writeData() is pinged a number of times as the WebM header is written;
    // the last time it is called it has the encoded data.
    const size_t encoded_data_size = GetParam().first_encoded_video_frame_size;
    EXPECT_CALL(*this, writeData(_, Lt(encoded_data_size), _))
        .Times(AtLeast(1));
    EXPECT_CALL(*this, writeData(_, encoded_data_size, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    // The second time around writeData() is called a number of times to write
    // the WebM frame header, and then is pinged with the encoded data.
    const size_t encoded_data_size = GetParam().second_encoded_video_frame_size;
    EXPECT_CALL(*this, writeData(_, Lt(encoded_data_size), _))
        .Times(AtLeast(1));
    EXPECT_CALL(*this, writeData(_, encoded_data_size, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    OnVideoFrameForTesting(video_frame);
    run_loop.Run();
  }

  media_recorder_handler_->stop();

  // Expect a last call on destruction, with size 0 and |lastInSlice| true.
  EXPECT_CALL(*this, writeData(nullptr, 0, true)).Times(1);
  media_recorder_handler_.reset();
}

INSTANTIATE_TEST_CASE_P(,
                        MediaRecorderHandlerTest,
                        ValuesIn(kMediaRecorderTestParams));

// Sends 2 frames and expect them as WebM contained encoded data in writeData().
TEST_P(MediaRecorderHandlerTest, EncodeAudioFrames) {
  // Audio-only test.
  if (GetParam().has_video)
    return;

  AddTracks();

  const WebString mime_type(base::UTF8ToUTF16("audio/opus"));
  EXPECT_TRUE(media_recorder_handler_->initialize(this, registry_.test_stream(),
                                                  mime_type));
  EXPECT_TRUE(media_recorder_handler_->start());

  InSequence s;
  const scoped_ptr<media::AudioBus> audio_bus1 = NextAudioBus();
  const scoped_ptr<media::AudioBus> audio_bus2 = NextAudioBus();

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LINEAR, media::CHANNEL_LAYOUT_STEREO,
      kTestAudioSampleRate, kTestAudioBitsPerSample,
      kTestAudioSampleRate * kTestAudioBufferDurationMS / 1000);
  SetAudioFormatForTesting(params);

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    // writeData() is pinged a number of times as the WebM header is written;
    // the last time it is called it has the encoded data.
    const size_t kEncodedDataSize = GetParam().first_encoded_audio_frame_size;
    EXPECT_CALL(*this, writeData(_, Lt(kEncodedDataSize), _)).Times(AtLeast(1));
    EXPECT_CALL(*this, writeData(_, kEncodedDataSize, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    OnAudioBusForTesting(*audio_bus1);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    // The second time around writeData() is called a number of times to write
    // the WebM frame header, and then is pinged with the encoded data.
    const size_t kSecondEncodedDataSize =
        GetParam().second_encoded_audio_frame_size;
    EXPECT_CALL(*this, writeData(_, Lt(kSecondEncodedDataSize), _))
        .Times(AtLeast(1));
    EXPECT_CALL(*this, writeData(_, kSecondEncodedDataSize, _))
        .Times(1)
        .WillOnce(RunClosure(quit_closure));

    OnAudioBusForTesting(*audio_bus2);
    run_loop.Run();
  }

  media_recorder_handler_->stop();

  // Expect a last call on destruction, with size 0 and |lastInSlice| true.
  EXPECT_CALL(*this, writeData(nullptr, 0, true)).Times(1);
  media_recorder_handler_.reset();
}

}  // namespace content
