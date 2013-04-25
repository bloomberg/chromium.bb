// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_DECODER_SELECTOR_H_
#define MEDIA_FILTERS_VIDEO_DECODER_SELECTOR_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecoderBuffer;
class DecryptingDemuxerStream;
class Decryptor;

// VideoDecoderSelector (creates if necessary and) initializes the proper
// VideoDecoder for a given DemuxerStream. If the given DemuxerStream is
// encrypted, a DecryptingDemuxerStream may also be created.
class MEDIA_EXPORT VideoDecoderSelector {
 public:
  // Indicates completion of VideoDecoder selection.
  // - First parameter: The initialized VideoDecoder. If it's set to NULL, then
  // VideoDecoder initialization failed.
  // - Second parameter: The initialized DecryptingDemuxerStream. If it's not
  // NULL, then a DecryptingDemuxerStream is created and initialized to do
  // decryption for the initialized VideoDecoder.
  // Note: The caller owns selected VideoDecoder and DecryptingDemuxerStream.
  // The caller should call DecryptingDemuxerStream::Reset() before
  // calling VideoDecoder::Reset() to release any pending decryption or read.
  typedef base::Callback<
      void(scoped_ptr<VideoDecoder>,
           scoped_ptr<DecryptingDemuxerStream>)> SelectDecoderCB;

  // |decoders| contains the VideoDecoders to use when initializing.
  //
  // |set_decryptor_ready_cb| is optional. If |set_decryptor_ready_cb| is null,
  // no decryptor will be available to perform decryption.
  VideoDecoderSelector(
      const scoped_refptr<base::MessageLoopProxy>& message_loop,
      ScopedVector<VideoDecoder> decoders,
      const SetDecryptorReadyCB& set_decryptor_ready_cb);
  ~VideoDecoderSelector();

  // Initializes and selects an VideoDecoder that can decode the |stream|.
  // Selected VideoDecoder (and DecryptingDemuxerStream) is returned via
  // the |select_decoder_cb|.
  void SelectVideoDecoder(DemuxerStream* stream,
                          const StatisticsCB& statistics_cb,
                          const SelectDecoderCB& select_decoder_cb);

 private:
  void DecryptingVideoDecoderInitDone(PipelineStatus status);
  void DecryptingDemuxerStreamInitDone(PipelineStatus status);
  void InitializeDecoder(ScopedVector<VideoDecoder>::iterator iter);
  void DecoderInitDone(ScopedVector<VideoDecoder>::iterator iter,
                       PipelineStatus status);

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  ScopedVector<VideoDecoder> decoders_;
  SetDecryptorReadyCB set_decryptor_ready_cb_;

  DemuxerStream* input_stream_;
  StatisticsCB statistics_cb_;
  SelectDecoderCB select_decoder_cb_;

  scoped_ptr<VideoDecoder> video_decoder_;
  scoped_ptr<DecryptingDemuxerStream> decrypted_stream_;

  base::WeakPtrFactory<VideoDecoderSelector> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(VideoDecoderSelector);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_DECODER_SELECTOR_H_
