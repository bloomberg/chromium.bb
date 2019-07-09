// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_CLIENTS_MOJO_DEMUXER_STREAM_IMPL_H_
#define MEDIA_MOJO_CLIENTS_MOJO_DEMUXER_STREAM_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace media {

class DemuxerStream;
class MojoDecoderBufferWriter;

// This class wraps a media::DemuxerStream and exposes it as a
// mojom::DemuxerStream for use as a proxy from remote applications.
class MojoDemuxerStreamImpl : public mojom::DemuxerStream {
 public:
  // |stream| is the underlying DemuxerStream we are proxying for.
  // Note: |this| does not take ownership of |stream|.
  MojoDemuxerStreamImpl(media::DemuxerStream* stream,
                        mojo::InterfaceRequest<mojom::DemuxerStream> request);
  ~MojoDemuxerStreamImpl() override;

  // mojom::DemuxerStream implementation.
  // InitializeCallback and ReadCallback are defined in
  // mojom::DemuxerStream.
  void Initialize(InitializeCallback callback) override;
  void Read(ReadCallback callback) override;
  void EnableBitstreamConverter() override;

  // Sets an error handler that will be called if a connection error occurs on
  // the bound message pipe.
  void set_connection_error_handler(const base::Closure& error_handler) {
    binding_.set_connection_error_handler(error_handler);
  }

 private:
  using Type = media::DemuxerStream::Type;
  using Status = media::DemuxerStream::Status;

  void OnBufferReady(ReadCallback callback,
                     Status status,
                     scoped_refptr<DecoderBuffer> buffer);

  mojo::Binding<mojom::DemuxerStream> binding_;

  // See constructor.  We do not own |stream_|.
  media::DemuxerStream* stream_;

  std::unique_ptr<MojoDecoderBufferWriter> mojo_decoder_buffer_writer_;

  base::WeakPtrFactory<MojoDemuxerStreamImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(MojoDemuxerStreamImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_MOJO_DEMUXER_STREAM_IMPL_H_
