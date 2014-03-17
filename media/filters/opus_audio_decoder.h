// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
#define MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"
#include "media/base/sample_format.h"

struct OpusMSDecoder;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class AudioBuffer;
class AudioTimestampHelper;
class DecoderBuffer;
struct QueuedAudioBuffer;

class MEDIA_EXPORT OpusAudioDecoder : public AudioDecoder {
 public:
  explicit OpusAudioDecoder(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  virtual ~OpusAudioDecoder();

  // AudioDecoder implementation.
  virtual void Initialize(const AudioDecoderConfig& config,
                          const PipelineStatusCB& status_cb) OVERRIDE;
  virtual void Decode(const scoped_refptr<DecoderBuffer>& buffer,
                      const DecodeCB& decode_cb) OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int samples_per_second() OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

 private:
  void DoReset();
  void DoStop();

  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& input,
                    const DecodeCB& decode_cb);

  bool ConfigureDecoder();
  void CloseDecoder();
  void ResetTimestampState();
  bool Decode(const scoped_refptr<DecoderBuffer>& input,
              scoped_refptr<AudioBuffer>* output_buffer);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<OpusAudioDecoder> weak_factory_;
  base::WeakPtr<OpusAudioDecoder> weak_this_;

  AudioDecoderConfig config_;
  OpusMSDecoder* opus_decoder_;

  // Decoded audio format.
  ChannelLayout channel_layout_;
  int samples_per_second_;
  const SampleFormat sample_format_;
  const int bits_per_channel_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  base::TimeDelta last_input_timestamp_;

  // Number of frames to be discarded from the start of the packet. This value
  // is respected for all packets except for the first one in the stream. For
  // the first packet in the stream, |frame_delay_at_start_| is used. This is
  // usually set to the SeekPreRoll value from the container whenever a seek
  // happens.
  int frames_to_discard_;

  // Number of frames to be discarded at the start of the stream. This value
  // is typically the CodecDelay value from the container.  This value should
  // only be applied when input timestamp is |start_input_timestamp_|.
  int frame_delay_at_start_;
  base::TimeDelta start_input_timestamp_;

  // Timestamp to be subtracted from all the frames. This is typically computed
  // from the CodecDelay value in the container.
  base::TimeDelta timestamp_offset_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(OpusAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
