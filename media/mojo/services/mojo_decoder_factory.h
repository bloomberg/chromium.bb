// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DECODER_FACTORY_H_
#define MEDIA_MOJO_SERVICES_MOJO_DECODER_FACTORY_H_

#include "base/macros.h"
#include "media/base/decoder_factory.h"

namespace media {

namespace interfaces {
class ServiceFactory;
}

class MojoDecoderFactory : public DecoderFactory {
 public:
  explicit MojoDecoderFactory(interfaces::ServiceFactory* service_factory);
  ~MojoDecoderFactory() final;

  void CreateAudioDecoders(ScopedVector<AudioDecoder>* audio_decoders) final;

  // TODO(xhwang): Add video decoder support if needed.

 private:
  interfaces::ServiceFactory* service_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoDecoderFactory);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DECODER_FACTORY_H_
