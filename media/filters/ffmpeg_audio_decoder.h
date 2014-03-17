// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
#define MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_

#include <list>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"
#include "media/ffmpeg/ffmpeg_deleters.h"

struct AVCodecContext;
struct AVFrame;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioTimestampHelper;
class DecoderBuffer;

class MEDIA_EXPORT FFmpegAudioDecoder : public AudioDecoder {
 public:
  explicit FFmpegAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  virtual ~FFmpegAudioDecoder();

  // AudioDecoder implementation.
  virtual void Initialize(const AudioDecoderConfig& config,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual scoped_refptr<AudioBuffer> GetDecodeOutput() OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int samples_per_second() OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished,
    kError
  };

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  // Handles decoding an unencrypted encoded buffer.
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                    const DecodeCB& decode_cb);
  bool FFmpegDecode(const scoped_refptr<DecoderBuffer>& buffer);

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true if initialization was successful.
  bool ConfigureDecoder();

  // Releases resources associated with |codec_context_| and |av_frame_|
  // and resets them to NULL.
  void ReleaseFFmpegResources();
  void ResetTimestampState();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<FFmpegAudioDecoder> weak_factory_;
  base::WeakPtr<FFmpegAudioDecoder> weak_this_;

  DecoderState state_;

  // FFmpeg structures owned by this object.
  scoped_ptr<AVCodecContext, ScopedPtrAVFreeContext> codec_context_;
  scoped_ptr<AVFrame, ScopedPtrAVFreeFrame> av_frame_;

  // Decoded audio format.
  int bytes_per_channel_;
  ChannelLayout channel_layout_;
  int channels_;
  int samples_per_second_;

  // AVSampleFormat initially requested; not Chrome's SampleFormat.
  int av_sample_format_;
  SampleFormat sample_format_;

  AudioDecoderConfig config_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  base::TimeDelta last_input_timestamp_;

  // Number of frames to drop before generating output buffers.
  int output_frames_to_drop_;

  // Since multiple frames may be decoded from the same packet we need to queue
  // them up.
  std::list<scoped_refptr<AudioBuffer> > queued_audio_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FFmpegAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AUDIO_DECODER_H_
