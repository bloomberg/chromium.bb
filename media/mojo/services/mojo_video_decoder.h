// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_H_
#define MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/video_decoder.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// A VideoDecoder that proxies to a mojom::VideoDecoder.
class MojoVideoDecoder : public VideoDecoder {
 public:
  MojoVideoDecoder();
  ~MojoVideoDecoder() final;

  // VideoDecoder implementation.
  std::string GetDisplayName() const final;
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) final;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) final;
  void Reset(const base::Closure& closure) final;
  bool NeedsBitstreamConversion() const final;
  bool CanReadWithoutStalling() const final;
  int GetMaxDecodeRequests() const final;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MojoVideoDecoder);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_VIDEO_DECODER_H_
