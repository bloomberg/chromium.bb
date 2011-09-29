// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "media/base/filter_factories.h"

namespace media {

class ChunkDemuxer;
class ChunkDemuxerClient;

// Factory for building ChunkDemuxers. The factory will only build a
// ChunkDemuxer for build URLs that match the one passed into the constructor.
// All other URLs are delegated to |delegate_factory_|. The url passed to
// the constructor represents the "special" URL that indicates that the
// ChunkDemuxer should be used for playback.
class MEDIA_EXPORT ChunkDemuxerFactory : public DemuxerFactory {
 public:
  // Takes ownership of |delegate_factory|.
  ChunkDemuxerFactory(const std::string& url, DemuxerFactory* delegate_factory,
                      ChunkDemuxerClient* client);
  virtual ~ChunkDemuxerFactory();

  // DemuxerFactory methods.
  virtual void Build(const std::string& url, const BuildCallback& cb);
  virtual DemuxerFactory* Clone() const;

 private:
  std::string url_;
  scoped_ptr<DemuxerFactory> delegate_factory_;
  ChunkDemuxerClient* client_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_
