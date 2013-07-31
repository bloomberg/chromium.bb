// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <list>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"

struct AVCodecContext;
struct AVFrame;

namespace base {
class MessageLoopProxy;
}

namespace media {

class AudioTimestampHelper;
class DecoderBuffer;
struct QueuedAudioBuffer;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  explicit FFmpegAudioDecoder(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);
  virtual ~FFmpegAudioDecoder();

  // AudioDecoder implementation.
  virtual void Initialize(DemuxerStream* stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int samples_per_second() OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;

  // Callback called from within FFmpeg to allocate a buffer based on
  // the dimensions of |codec_context|. See AVCodecContext.get_buffer2
  // documentation inside FFmpeg.
  int GetAudioBuffer(AVCodecContext* codec, AVFrame* frame, int flags);

 private:
  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& input);

  bool ConfigureDecoder();
  void ReleaseFFmpegResources();
  void ResetTimestampState();
  void RunDecodeLoop(const scoped_refptr<DecoderBuffer>& input,
                     bool skip_eos_append);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<FFmpegAudioDecoder> weak_factory_;
  base::WeakPtr<FFmpegAudioDecoder> weak_this_;

  DemuxerStream* demuxer_stream_;
  StatisticsCB statistics_cb_;
  AVCodecContext* codec_context_;

  // Decoded audio format.
  int bytes_per_channel_;
  ChannelLayout channel_layout_;
  int channels_;
  int samples_per_second_;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_;
  SampleFormat sample_format_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  base::TimeDelta last_input_timestamp_;

  // Number of frames to drop before generating output buffers.
  int output_frames_to_drop_;

  // Holds decoded audio.
  AVFrame* av_frame_;

  ReadCB read_cb_;

  // Since multiple frames may be decoded from the same packet we need to queue
  // them up and hand them out as we receive Read() calls.
  std::list<QueuedAudioBuffer> queued_audio_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
