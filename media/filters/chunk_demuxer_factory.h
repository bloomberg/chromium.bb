// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_
#define MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_

#include "base/scoped_ptr.h"
#include "media/base/filter_factories.h"

namespace media {

class ChunkDemuxer;

// Class used by an external object to send media data to the
// Demuxer. This object is created by the DemuxerFactory and
// contains the Demuxer that will be returned in the next Build()
// call on the factory. The external object tells the factory
// to create one of these objects before it starts the Pipeline.
// It does this because the external object may need to make AddData()
// calls before the pipeline has completely initialized. This class
// allows data from these calls to be queued until initialization
// completes. It represents the minimal operations needed by
// the external object to talk to the Demuxer. It also allows
// the external object to have a longer lifetime than the pipeline.
class MediaDataSink {
 public:
  MediaDataSink(const scoped_refptr<ChunkDemuxer>& demuxer);
  ~MediaDataSink();

  // Flush all data passed via AddData().
  void Flush();

  // Sends media data to the demuxer. Returns true if the data is valid.
  bool AddData(const uint8* data, unsigned length);

  // Signals that playback is shutting down and further AddData() calls
  // should fail. This also cancels pending Read()s on DemuxerStreams.
  void Shutdown();

 private:
  scoped_refptr<ChunkDemuxer> demuxer_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(MediaDataSink);
};

class ChunkDemuxerFactory : public DemuxerFactory {
 public:
  // Takes a reference to |demuxer_factory|.
  ChunkDemuxerFactory(DataSourceFactory* data_source_factory);
  virtual ~ChunkDemuxerFactory();

  bool IsUrlSupported(const std::string& url) const;
  MediaDataSink* CreateMediaDataSink();

  // DemuxerFactory methods.
  virtual void Build(const std::string& url, BuildCallback* cb);
  virtual DemuxerFactory* Clone() const;

 private:
  static const char kURLPrefix[];
  class BuildState;

  scoped_ptr<DataSourceFactory> data_source_factory_;
  scoped_refptr<ChunkDemuxer> demuxer_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ChunkDemuxerFactory);
};

}  // namespace media

#endif  // MEDIA_FILTERS_CHUNK_DEMUXER_FACTORY_H_
