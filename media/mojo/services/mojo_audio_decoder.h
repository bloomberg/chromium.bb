// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_H_
#define MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "media/base/audio_decoder.h"
#include "media/mojo/interfaces/audio_decoder.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// An AudioDecoder that proxies to an interfaces::AudioDecoder.
class MojoAudioDecoder : public AudioDecoder {
 public:
  MojoAudioDecoder(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                   interfaces::AudioDecoderPtr remote_decoder);
  ~MojoAudioDecoder() final;

  // AudioDecoder implementation.
  std::string GetDisplayName() const final;
  void Initialize(const AudioDecoderConfig& config,
                  CdmContext* cdm_context,
                  const InitCB& init_cb,
                  const OutputCB& output_cb) final;
  void Decode(const scoped_refptr<DecoderBuffer>& buffer,
              const DecodeCB& decode_cb) final;
  void Reset(const base::Closure& closure) final;
  bool NeedsBitstreamConversion() const final;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  interfaces::AudioDecoderPtr remote_decoder_;

  DISALLOW_COPY_AND_ASSIGN(MojoAudioDecoder);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_AUDIO_DECODER_H_
