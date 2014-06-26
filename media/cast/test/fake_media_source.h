// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A fake media source that generates video and audio frames to a cast
// sender.
// This class can transcode a WebM file using FFmpeg. It can also
// generate an animation and audio of fixed frequency.

#ifndef MEDIA_CAST_TEST_FAKE_MEDIA_SOURCE_H_
#define MEDIA_CAST_TEST_FAKE_MEDIA_SOURCE_H_

#include <queue>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "media/audio/audio_parameters.h"
#include "media/cast/cast_config.h"
#include "media/filters/audio_renderer_algorithm.h"
#include "media/filters/ffmpeg_demuxer.h"

struct AVCodecContext;
struct AVFormatContext;

namespace media {

class AudioBus;
class AudioFifo;
class AudioTimestampHelper;
class FFmpegGlue;
class InMemoryUrlProtocol;
class MultiChannelResampler;

namespace cast {

class AudioFrameInput;
class VideoFrameInput;
class TestAudioBusFactory;

class FakeMediaSource {
 public:
  // |task_runner| is to schedule decoding tasks.
  // |clock| is used by this source but is not owned.
  // |video_config| is the desired video config.
  FakeMediaSource(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                  base::TickClock* clock,
                  const VideoSenderConfig& video_config);
  ~FakeMediaSource();

  // Transcode this file as the source of video and audio frames.
  // If |override_fps| is non zero then the file is played at the desired rate.
  void SetSourceFile(const base::FilePath& video_file, int override_fps);

  void Start(scoped_refptr<AudioFrameInput> audio_frame_input,
             scoped_refptr<VideoFrameInput> video_frame_input);

  const VideoSenderConfig& get_video_config() const { return video_config_; }

 private:
  bool is_transcoding_audio() const { return audio_stream_index_ >= 0; }
  bool is_transcoding_video() const { return video_stream_index_ >= 0; }

  void SendNextFrame();
  void SendNextFakeFrame();

  // Return true if a frame was sent.
  bool SendNextTranscodedVideo(base::TimeDelta elapsed_time);

  // Return true if a frame was sent.
  bool SendNextTranscodedAudio(base::TimeDelta elapsed_time);

  // Helper methods to compute timestamps for the frame number specified.
  base::TimeDelta VideoFrameTime(int frame_number);

  base::TimeDelta ScaleTimestamp(base::TimeDelta timestamp);

  base::TimeDelta AudioFrameTime(int frame_number);

  // Go to the beginning of the stream.
  void Rewind();

  // Call FFmpeg to fetch one packet.
  ScopedAVPacket DemuxOnePacket(bool* audio);

  void DecodeAudio(ScopedAVPacket packet);
  void DecodeVideo(ScopedAVPacket packet);
  void Decode(bool decode_audio);

  void ProvideData(int frame_delay, media::AudioBus* output_bus);

  AVStream* av_audio_stream();
  AVStream* av_video_stream();
  AVCodecContext* av_audio_context();
  AVCodecContext* av_video_context();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  VideoSenderConfig video_config_;
  scoped_refptr<AudioFrameInput> audio_frame_input_;
  scoped_refptr<VideoFrameInput> video_frame_input_;
  uint8 synthetic_count_;
  base::TickClock* const clock_;  // Not owned by this class.

  // Time when the stream starts.
  base::TimeTicks start_time_;

  // The following three members are used only for fake frames.
  int audio_frame_count_;  // Each audio frame is exactly 10ms.
  int video_frame_count_;
  scoped_ptr<TestAudioBusFactory> audio_bus_factory_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<FakeMediaSource> weak_factory_;

  base::MemoryMappedFile file_data_;
  scoped_ptr<InMemoryUrlProtocol> protocol_;
  scoped_ptr<FFmpegGlue> glue_;
  AVFormatContext* av_format_context_;

  int audio_stream_index_;
  AudioParameters audio_params_;
  double playback_rate_;

  int video_stream_index_;
  int video_frame_rate_numerator_;
  int video_frame_rate_denominator_;

  // These are used for audio resampling.
  scoped_ptr<media::MultiChannelResampler> audio_resampler_;
  scoped_ptr<media::AudioFifo> audio_fifo_;
  scoped_ptr<media::AudioBus> audio_fifo_input_bus_;
  media::AudioRendererAlgorithm audio_algo_;

  // Track the timestamp of audio sent to the receiver.
  scoped_ptr<media::AudioTimestampHelper> audio_sent_ts_;

  std::queue<scoped_refptr<VideoFrame> > video_frame_queue_;
  int64 video_first_pts_;
  bool video_first_pts_set_;

  std::queue<AudioBus*> audio_bus_queue_;

  DISALLOW_COPY_AND_ASSIGN(FakeMediaSource);
};

}  // namespace cast
}  // namespace media

#endif // MEDIA_CAST_TEST_FAKE_MEDIA_SOURCE_H_
