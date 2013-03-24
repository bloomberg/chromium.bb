// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_FRAME_STREAM_H_
#define MEDIA_FILTERS_VIDEO_FRAME_STREAM_H_

#include <list>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "media/base/decryptor.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder.h"

namespace base {
class MessageLoopProxy;
}

namespace media {

class DecryptingDemuxerStream;
class DemuxerStream;
class VideoDecoderSelector;

// Wraps a DemuxerStream and a list of VideoDecoders and provides decoded
// VideoFrames to its client (e.g. VideoRendererBase).
class MEDIA_EXPORT VideoFrameStream {
 public:
  typedef std::list<scoped_refptr<VideoDecoder> > VideoDecoderList;

  // Indicates completion of VideoFrameStream initialization.
  typedef base::Callback<void(bool success, bool has_alpha)> InitCB;

  VideoFrameStream(const scoped_refptr<base::MessageLoopProxy>& message_loop,
                   const SetDecryptorReadyCB& set_decryptor_ready_cb);

  ~VideoFrameStream();

  // Initializes the VideoFrameStream and returns the initialization result
  // through |init_cb|. Note that |init_cb| is always called asynchronously.
  void Initialize(const scoped_refptr<DemuxerStream>& stream,
                  const VideoDecoderList& decoders,
                  const StatisticsCB& statistics_cb,
                  const InitCB& init_cb);

  // Reads a decoded VideoFrame and returns it via the |read_cb|. Note that
  // |read_cb| is always called asynchronously. This method should only be
  // called after initialization has succeeded and must not be called during
  // any pending Reset() and/or Stop().
  void ReadFrame(const VideoDecoder::ReadCB& read_cb);

  // Resets the decoder, flushes all decoded frames and/or internal buffers,
  // fires any existing pending read callback and calls |closure| on completion.
  // Note that |closure| is always called asynchronously. This method should
  // only be called after initialization has succeeded and must not be called
  // during any pending Reset() and/or Stop().
  void Reset(const base::Closure& closure);

  // Stops the decoder, fires any existing pending read callback or reset
  // callback and calls |closure| on completion. Note that |closure| is always
  // called asynchronously. The VideoFrameStream cannot be used anymore after
  // it is stopped. This method can be called at any time but not during another
  // pending Stop().
  void Stop(const base::Closure& closure);

  // Returns true if the decoder currently has the ability to decode and return
  // a VideoFrame.
  bool HasOutputFrameAvailable() const;

 private:
  enum State {
    UNINITIALIZED,
    NORMAL,
    STOPPED
  };

  // Called when |decoder_selector_| selected the |selected_decoder|.
  // |decrypting_demuxer_stream| was also populated if a DecryptingDemuxerStream
  // is created to help decrypt the encrypted stream.
  // Note: |decoder_selector| is passed here to keep the VideoDecoderSelector
  // alive until OnDecoderSelected() finishes.
  void OnDecoderSelected(
      scoped_ptr<VideoDecoderSelector> decoder_selector,
      const scoped_refptr<VideoDecoder>& selected_decoder,
      const scoped_refptr<DecryptingDemuxerStream>& decrypting_demuxer_stream);

  // Callback for VideoDecoder::Read().
  void OnFrameRead(const VideoDecoder::Status status,
                   const scoped_refptr<VideoFrame>& frame);

  void ResetDecoder();
  void OnDecoderReset();

  void StopDecoder();
  void OnDecoderStopped();

  scoped_refptr<base::MessageLoopProxy> message_loop_;
  base::WeakPtrFactory<VideoFrameStream> weak_factory_;
  base::WeakPtr<VideoFrameStream> weak_this_;

  State state_;

  InitCB init_cb_;
  VideoDecoder::ReadCB read_cb_;
  base::Closure reset_cb_;
  base::Closure stop_cb_;

  SetDecryptorReadyCB set_decryptor_ready_cb_;

  // These two will be set by VideoDecoderSelector::SelectVideoDecoder().
  scoped_refptr<VideoDecoder> decoder_;
  scoped_refptr<DecryptingDemuxerStream> decrypting_demuxer_stream_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameStream);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_FRAME_STREAM_H_
