// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
#define MEDIA_FILTERS_VPX_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/demuxer_stream.h"
#include "media/base/video_decoder.h"
#include "media/base/video_frame.h"

// Include libvpx header files.
// VPX_CODEC_DISABLE_COMPAT excludes parts of the libvpx API that provide
// backwards compatibility for legacy applications using the library.
#define VPX_CODEC_DISABLE_COMPAT 1
extern "C" {
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
}

struct vpx_codec_ctx;
struct vpx_image;

namespace base {
class MessageLoopProxy;
}

namespace media {

struct VpxDeleter {
  inline void operator()(vpx_codec_ctx* ptr) const {
    if (ptr) {
      vpx_codec_destroy(ptr);
      delete ptr;
    }
  }
};

class MEDIA_EXPORT VpxVideoDecoder : public VideoDecoder {
 public:
  explicit VpxVideoDecoder(
      const scoped_refptr<base::MessageLoopProxy>& message_loop);

  // VideoDecoder implementation.
  virtual void Initialize(const scoped_refptr<DemuxerStream>& stream,
                          const PipelineStatusCB& status_cb,
                          const StatisticsCB& statistics_cb) OVERRIDE;
  virtual void Read(const ReadCB& read_cb) OVERRIDE;
  virtual void Reset(const base::Closure& closure) OVERRIDE;
  virtual void Stop(const base::Closure& closure) OVERRIDE;

 protected:
  virtual ~VpxVideoDecoder();

 private:
  enum DecoderState {
    kUninitialized,
    kNormal,
    kFlushCodec,
    kDecodeFinished
  };

  // Handles (re-)initializing the decoder with a (new) config.
  // Returns true when initialization was successful.
  bool ConfigureDecoder();

  void ReadFromDemuxerStream();

  // Carries out the buffer processing operation scheduled by
  // DecryptOrDecodeBuffer().
  void DoDecryptOrDecodeBuffer(DemuxerStream::Status status,
                               const scoped_refptr<DecoderBuffer>& buffer);

  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer);
  bool Decode(const scoped_refptr<DecoderBuffer>& buffer,
              scoped_refptr<VideoFrame>* video_frame);

  // Reset decoder and call |reset_cb_|.
  void DoReset();

  void CopyVpxImageTo(const struct vpx_image* vpx_image,
                      const struct vpx_image* vpx_image_alpha,
                      scoped_refptr<VideoFrame>* video_frame);

  scoped_refptr<base::MessageLoopProxy> message_loop_;

  DecoderState state_;

  StatisticsCB statistics_cb_;
  ReadCB read_cb_;
  base::Closure reset_cb_;

  // Pointer to the demuxer stream that will feed us compressed buffers.
  scoped_refptr<DemuxerStream> demuxer_stream_;

  scoped_ptr<vpx_codec_ctx, VpxDeleter> vpx_codec_;
  scoped_ptr<vpx_codec_ctx, VpxDeleter> vpx_codec_alpha_;

  DISALLOW_COPY_AND_ASSIGN(VpxVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_FILTERS_VPX_VIDEO_DECODER_H_
