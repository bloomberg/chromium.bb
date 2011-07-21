// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_CLIENT_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_CLIENT_H_

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
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_CLIENT_H_
