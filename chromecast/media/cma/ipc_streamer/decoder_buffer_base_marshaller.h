// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_IPC_STREAMER_DECODER_BUFFER_BASE_MARSHALLER_H_
#define CHROMECAST_MEDIA_CMA_IPC_STREAMER_DECODER_BUFFER_BASE_MARSHALLER_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

namespace chromecast {
namespace media {
class DecoderBufferBase;
class MediaMessage;

class DecoderBufferBaseMarshaller {
 public:
  // Writes the serialized structure of |config| into |msg|.
  static void Write(
      const scoped_refptr<DecoderBufferBase>& buffer, MediaMessage* msg);

  // Returns a decoder buffer from its serialized structure.
  static scoped_refptr<DecoderBufferBase> Read(scoped_ptr<MediaMessage> msg);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_IPC_STREAMER_DECODER_BUFFER_BASE_MARSHALLER_H_
