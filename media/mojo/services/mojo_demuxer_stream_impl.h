// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_IMPL_H_
#define MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_IMPL_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/demuxer_stream.h"
#include "media/mojo/interfaces/demuxer_stream.mojom.h"
#include "mojo/public/cpp/bindings/interface_impl.h"

namespace media {
class DemuxerStream;

// This class wraps a media::DemuxerStream and exposes it as a
// mojo::DemuxerStream for use as a proxy from remote applications.
class MojoDemuxerStreamImpl : public mojo::InterfaceImpl<mojo::DemuxerStream> {
 public:
  // |stream| is the underlying DemuxerStream we are proxying for.
  // Note: |this| does not take ownership of |stream|.
  explicit MojoDemuxerStreamImpl(media::DemuxerStream* stream);
  virtual ~MojoDemuxerStreamImpl();

  // mojo::DemuxerStream implementation.
  virtual void Read(const mojo::Callback<
      void(mojo::DemuxerStream::Status, mojo::MediaDecoderBufferPtr)>& callback)
      OVERRIDE;

  // mojo::InterfaceImpl overrides.
  virtual void OnConnectionEstablished() OVERRIDE;

 private:
  // |callback| is the callback that was passed to the initiating Read()
  //     call by our client.
  // |status| and |buffer| are the standard media::ReadCB parameters.
  typedef mojo::Callback<void(mojo::DemuxerStream::Status,
                              mojo::MediaDecoderBufferPtr)> BufferReadyCB;
  void OnBufferReady(const BufferReadyCB& callback,
                     media::DemuxerStream::Status status,
                     const scoped_refptr<media::DecoderBuffer>& buffer);

  // See constructor.  We do not own |stream_|.
  media::DemuxerStream* stream_;

  base::WeakPtrFactory<MojoDemuxerStreamImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(MojoDemuxerStreamImpl);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_DEMUXER_STREAM_IMPL_H_
