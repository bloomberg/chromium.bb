// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/demuxer_factory.h"

namespace media {

class ChunkDemuxer;
class ChunkDemuxerClient;

// Factory for building ChunkDemuxers. The factory will only build a
// ChunkDemuxer with the given client.
class MEDIA_EXPORT ChunkDemuxerFactory : public DemuxerFactory {
 public:
  ChunkDemuxerFactory(ChunkDemuxerClient* client);
  virtual ~ChunkDemuxerFactory();

  // DemuxerFactory methods.
  virtual void Build(const std::string& url, const BuildCallback& cb) OVERRIDE;

 private:
  ChunkDemuxerClient* client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_
