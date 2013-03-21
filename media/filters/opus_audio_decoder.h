// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
#define MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "media/base/audio_decoder.h"
#include "media/base/demuxer_stream.h"

struct OpusMSDecoder;

namespace base {
class MessageLoopProxy;
}

namespace media {

class AudioTimestampHelper;
class DataBuffer;
class DecoderBuffer;
struct QueuedAudioBuffer;

class MEDIA_EXPORT OpusAudioDecoder : public AudioDecoder {
 public:
  explicit OpusAudioDecoder(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);
  virtual ~OpusAudioDecoder();

  // AudioDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual int bits_per_channel() OVERRIDE;
  virtual ChannelLayout channel_layout() OVERRIDE;
  virtual int samples_per_second() OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;

 private:
  // Reads from the demuxer stream with corresponding callback method.
  void ReadFromDemuxerStream();
  void BufferReady(DemuxerStream::Status status,
                   const scoped_refptr<DecoderBuffer>& input);


  bool ConfigureDecoder();
  void CloseDecoder();
  void ResetTimestampState();
  bool Decode(const scoped_refptr<DecoderBuffer>& input,
              scoped_refptr<DataBuffer>* output_buffer);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<OpusAudioDecoder> weak_factory_;
  base::WeakPtr<OpusAudioDecoder> weak_this_;

  scoped_refptr<DemuxerStream> demuxer_stream_;
  StatisticsCB statistics_cb_;
  OpusMSDecoder* opus_decoder_;

  // Decoded audio format.
  int bits_per_channel_;
  ChannelLayout channel_layout_;
  int samples_per_second_;

  // Used for computing output timestamps.
  scoped_ptr<AudioTimestampHelper> output_timestamp_helper_;
  base::TimeDelta last_input_timestamp_;

  // Number of output sample bytes to drop before generating
  // output buffers.
  int output_bytes_to_drop_;

  ReadCB read_cb_;

  int skip_samples_;

  // Buffer for output from libopus.
  scoped_array<int16> output_buffer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(OpusAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_OPUS_AUDIO_DECODER_H_
