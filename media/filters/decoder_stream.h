// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_DECODER_STREAM_H_
#define MEDIA_FILTERS_DECODER_STREAM_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/filters/decoder_selector.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

class DecryptingDemuxerStream;

// Wraps a DemuxerStream and a list of Decoders and provides decoded
// output to its client (e.g. Audio/VideoRendererImpl).
template<DemuxerStream::Type StreamType>
class MEDIA_EXPORT DecoderStream {
 public:
  typedef DecoderStreamTraits<StreamType> StreamTraits;
  typedef typename StreamTraits::DecoderType Decoder;
  typedef typename StreamTraits::OutputType Output;
  typedef typename StreamTraits::StreamInitCB InitCB;

  enum Status {
    OK,  // Everything went as planned.
    ABORTED,  // Read aborted due to Reset() during pending read.
    DEMUXER_READ_ABORTED,  // Demuxer returned aborted read.
    DECODE_ERROR,  // Decoder returned decode error.
    DECRYPT_ERROR  // Decoder returned decrypt error.
  };

  // Indicates completion of a DecoderStream read.
  typedef base::Callback<void(Status, const scoped_refptr<Output>&)> ReadCB;

  DecoderStream(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      ScopedVector<Decoder> decoders,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  virtual ~DecoderStream();

  // Initializes the DecoderStream and returns the initialization result
  // through |init_cb|. Note that |init_cb| is always called asynchronously.
  void Initialize(DemuxerStream* stream,
                  const StatisticsCB& statistics_cb,
                  const InitCB& init_cb);

  // Reads a decoded Output and returns it via the |read_cb|. Note that
  // |read_cb| is always called asynchronously. This method should only be
  // called after initialization has succeeded and must not be called during
  // any pending Reset() and/or Stop().
  void Read(const ReadCB& read_cb);

  // Resets the decoder, flushes all decoded outputs and/or internal buffers,
  // fires any existing pending read callback and calls |closure| on completion.
  // Note that |closure| is always called asynchronously. This method should
  // only be called after initialization has succeeded and must not be called
  // during any pending Reset() and/or Stop().
  void Reset(const base::Closure& closure);

  // Stops the decoder, fires any existing pending read callback or reset
  // callback and calls |closure| on completion. Note that |closure| is always
  // called asynchronously. The DecoderStream cannot be used anymore after
  // it is stopped. This method can be called at any time but not during another
  // pending Stop().
  void Stop(const base::Closure& closure);

  // Returns true if the decoder currently has the ability to decode and return
  // an Output.
  bool CanReadWithoutStalling() const;

 private:
  enum State {
    STATE_UNINITIALIZED,
    STATE_INITIALIZING,
    STATE_NORMAL,  // Includes idle, pending decoder decode/reset/stop.
    STATE_FLUSHING_DECODER,
    STATE_PENDING_DEMUXER_READ,
    STATE_REINITIALIZING_DECODER,
    STATE_STOPPED,
    STATE_ERROR
  };

  // Called when |decoder_selector| selected the |selected_decoder|.
  // |decrypting_demuxer_stream| was also populated if a DecryptingDemuxerStream
  // is created to help decrypt the encrypted stream.
  void OnDecoderSelected(
      scoped_ptr<Decoder> selected_decoder,
      scoped_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream);

  // Satisfy pending |read_cb_| with |status| and |output|.
  void SatisfyRead(Status status, const scoped_refptr<Output>& output);

  // Abort pending |read_cb_|.
  void AbortRead();

  // Decodes |buffer| and returns the result via OnDecodeOutputReady().
  void Decode(const scoped_refptr<DecoderBuffer>& buffer);

  // Flushes the decoder with an EOS buffer to retrieve internally buffered
  // decoder output.
  void FlushDecoder();

  // Callback for Decoder::Decode().
  void OnDecodeOutputReady(int buffer_size,
                           typename Decoder::Status status,
                           const scoped_refptr<Output>& output);

  // Reads a buffer from |stream_| and returns the result via OnBufferReady().
  void ReadFromDemuxerStream();

  // Callback for DemuxerStream::Read().
  void OnBufferReady(DemuxerStream::Status status,
                     const scoped_refptr<DecoderBuffer>& buffer);

  void ReinitializeDecoder();

  // Callback for Decoder reinitialization.
  void OnDecoderReinitialized(PipelineStatus status);

  void ResetDecoder();
  void OnDecoderReset();

  void StopDecoder();
  void OnDecoderStopped();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::WeakPtrFactory<DecoderStream<StreamType> > weak_factory_;

  State state_;

  StatisticsCB statistics_cb_;
  InitCB init_cb_;

  ReadCB read_cb_;
  base::Closure reset_cb_;
  base::Closure stop_cb_;

  DemuxerStream* stream_;

  scoped_ptr<DecoderSelector<StreamType> > decoder_selector_;

  // These two will be set by DecoderSelector::SelectDecoder().
  scoped_ptr<Decoder> decoder_;
  scoped_ptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  // This is required so the VideoFrameStream can access private members in
  // FinishInitialization() and ReportStatistics().
  DISALLOW_IMPLICIT_CONSTRUCTORS(DecoderStream);
};

typedef DecoderStream<DemuxerStream::VIDEO> VideoFrameStream;

}  // namespace media

#endif  // MEDIA_FILTERS_DECODER_STREAM_H_
