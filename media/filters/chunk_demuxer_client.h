// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_CLIENT_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_CLIENT_H_

#include "base/memory/scoped_ptr.h"

namespace media {

class ChunkDemuxer;

// Interface used to notify an external object when a ChunkDemuxer
// is opened & closed.
class ChunkDemuxerClient {
 public:
  // Called when a ChunkDemuxer object is opened.
  virtual void DemuxerOpened(ChunkDemuxer* demuxer) = 0;

  // The ChunkDemuxer passed via last DemuxerOpened() call is now
  // closed. Any further calls on the demuxer will result in an error.
  virtual void DemuxerClosed() = 0;

  // A decryption key associated with |init_data| may be needed to decrypt the
  // media being demuxed before decoding. Note that the demuxing itself does not
  // need decryption.
  virtual void DemuxerNeedKey(scoped_array<uint8> init_data,
                              int init_data_size) = 0;

 protected:
  virtual ~ChunkDemuxerClient() {}
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_CLIENT_H_
