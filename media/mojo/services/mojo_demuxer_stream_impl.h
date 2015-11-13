// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_IMPL_H_
#define MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace media {
class DemuxerStream;

// This class wraps a media::DemuxerStream and exposes it as a
// interfaces::DemuxerStream for use as a proxy from remote applications.
class MojoDemuxerStreamImpl : public interfaces::DemuxerStream {
 public:
  // |stream| is the underlying DemuxerStream we are proxying for.
  // Note: |this| does not take ownership of |stream|.
  MojoDemuxerStreamImpl(
      media::DemuxerStream* stream,
      mojo::InterfaceRequest<interfaces::DemuxerStream> request);
  ~MojoDemuxerStreamImpl() override;

  // interfaces::DemuxerStream implementation.
  // InitializeCallback and ReadCallback are defined in
  // interfaces::DemuxerStream.
  void Initialize(const InitializeCallback& callback) override;
  void Read(const ReadCallback& callback) override;

 private:
  void OnBufferReady(const ReadCallback& callback,
                     media::DemuxerStream::Status status,
                     const scoped_refptr<media::DecoderBuffer>& buffer);

  mojo::StrongBinding<interfaces::DemuxerStream> binding_;

  // See constructor.  We do not own |stream_|.
  media::DemuxerStream* stream_;

  // DataPipe for serializing the data section of DecoderBuffer into.
  mojo::ScopedDataPipeProducerHandle stream_pipe_;

  base::WeakPtrFactory<MojoDemuxerStreamImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MojoDemuxerStreamImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_IMPL_H_
